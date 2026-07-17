// MIME corpus runner: parse every *.eml file under one or more directories
// through the full pipeline and report aggregate statistics. Exits non-zero
// when the parse success rate falls below --min-success (default 1.0), so it
// doubles as a CI gate and a benchmark harness.
//
//   mime_corpus [--min-success 0.95] [--quiet] [--mbox] DIR [DIR...]
//
// --mbox: real corpora (SpamAssassin, Enron) store messages in mbox format,
// not one file per message: each file may itself contain N concatenated
// RFC 5322 messages separated by a "From " envelope line (issue #9). Without
// --mbox, every regular file is treated as exactly one message (unchanged
// default behavior). With --mbox, each file is first split into individual
// messages (see split_mbox below) and every resulting message is parsed and
// counted independently; --min-success then applies to messages, not files.

#include <libglot/mime/mime.h>
#include <libglot/util/arena.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace mime = libglot::mime;

namespace {

struct Stats {
    size_t total = 0;
    size_t parsed = 0;    // parse_message returned a message
    size_t rejected = 0;  // rejected by anomaly policy
    size_t threw = 0;     // ParseError (malformed header section)
    size_t text_decoded = 0;
    size_t text_parts = 0;
    std::map<std::string, size_t> anomalies; // by kind name
};

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ============================================================================
// mbox splitting (issue #9)
// ============================================================================
//
// A separator is a line beginning with "From " (the mbox "From_" envelope
// line, e.g. "From user@example.com  Wed Aug 21 13:52:33 2002") positioned
// at the very start of the file, or immediately after a blank line -- the
// standard mbox heuristic (see e.g. qmail-mbox(5), Python's `mailbox`
// module). The separator line itself is discarded: it is a storage
// envelope, not RFC 5322 content. A naive "strip line 1" is wrong because
// at least one real corpus file carries a "From " line mid-file, i.e. it is
// genuinely a multi-message mbox that must be SPLIT, not have one line
// removed.
//
// Body-quoting convention: **mboxrd**. To keep a body line that happens to
// start with "From " from being misread as a separator, mbox writers quote
// it by prepending '>'. mboxrd's rule is recursive: any line already
// matching /^>*From / gets exactly one more '>' when written, so
// "From x" -> ">From x" -> ">>From x" -> ... . This function reverses that
// by stripping exactly one leading '>' from any line matching /^>+From /.
//
// mboxrd was chosen over the older mboxo convention (which quotes only a
// bare "From " line, i.e. /^From /, and never re-quotes an already-quoted
// line) because mboxo is not losslessly reversible: given a body line
// ">From x", mboxo cannot tell whether that was originally "From x" (quoted
// once) or already ">From x" verbatim (e.g. a quoted reply to a message
// that itself contained a "From " line) -- both look identical after mboxo
// quoting. mboxrd's recursive quoting removes that ambiguity, at the cost
// of assuming the corpus was itself written with mboxrd semantics; this is
// the modern, widely-documented default (qmail, most current MUAs/MTAs) and
// is the safer choice for a corpus of unknown provenance, since real "From "
// body lines are otherwise rare and the recursive rule degrades gracefully
// (a line with no leading '>' is never touched).
std::vector<std::string> split_mbox(const std::string& raw) {
    std::vector<std::string> messages;
    std::string current;
    bool have_current = false;
    bool prev_blank = true; // start-of-file counts as "after a blank line"

    size_t pos = 0;
    const size_t n = raw.size();
    while (pos < n) {
        const size_t nl = raw.find('\n', pos);
        const bool has_nl = (nl != std::string::npos);
        const size_t line_end = has_nl ? nl : n;
        std::string_view line(raw.data() + pos, line_end - pos); // excludes '\n'

        std::string_view trimmed = line;
        if (!trimmed.empty() && trimmed.back() == '\r') {
            trimmed.remove_suffix(1);
        }
        const bool is_blank = trimmed.empty();
        const bool is_separator = prev_blank && trimmed.rfind("From ", 0) == 0;

        if (is_separator) {
            if (have_current) {
                messages.push_back(current);
                current.clear();
            }
            have_current = true;
        } else if (have_current || !is_blank) {
            // Leading blank line(s) before the first separator (or before
            // any content, for a plain non-mbox file run with --mbox) carry
            // no content and are dropped rather than becoming a spurious
            // empty leading message.
            have_current = true;

            size_t quote_len = 0;
            while (quote_len < trimmed.size() && trimmed[quote_len] == '>') {
                ++quote_len;
            }
            const bool quoted_from =
                quote_len > 0 && trimmed.substr(quote_len).rfind("From ", 0) == 0;

            current.append(quoted_from ? line.substr(1) : line);
            if (has_nl) {
                current.push_back('\n');
            }
        }

        prev_blank = is_blank;
        pos = has_nl ? nl + 1 : n;
    }

    if (have_current) {
        messages.push_back(current);
    }
    return messages;
}

void walk_message(const mime::Message& msg, Stats& s) {
    if (const auto* ct = mime::find_header(msg, "Content-Type")) {
        if (ct->value.rfind("text/", 0) == 0) {
            ++s.text_parts;
            if (mime::decoded_body_utf8(msg)) {
                ++s.text_decoded;
            }
        }
    }
    for (const auto* part : msg.parts) {
        if (part != nullptr) {
            walk_message(*part, s);
        }
    }
}

/// Parse one already-extracted RFC 5322 message and fold the result into
/// `s`. Shared by the plain (one file = one message) and --mbox (one file =
/// N split messages) modes.
void process_one_message(const std::string& raw, Stats& s) {
    ++s.total;
    libglot::Arena arena;
    try {
        const mime::ParseResult r = mime::parse_message(arena, raw);
        if (r.rejected) {
            ++s.rejected;
        }
        if (r.message != nullptr) {
            ++s.parsed;
            walk_message(*r.message, s);
        }
        for (const auto& rec : r.report.records) {
            s.anomalies[std::string(mime::anomaly_kind_name(rec.kind))]++;
        }
    } catch (const libglot::ParseError&) {
        ++s.threw;
    }
}

} // namespace

int main(int argc, char** argv) {
    double min_success = 1.0;
    bool quiet = false;
    bool mbox = false;
    std::vector<fs::path> dirs;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--min-success" && i + 1 < argc) {
            min_success = std::atof(argv[++i]);
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--mbox") {
            mbox = true;
        } else {
            dirs.emplace_back(a);
        }
    }
    if (dirs.empty()) {
        std::fprintf(stderr, "usage: mime_corpus [--min-success F] [--quiet] [--mbox] DIR...\n");
        return 2;
    }

    Stats s;
    for (const auto& dir : dirs) {
        if (!fs::exists(dir)) {
            std::fprintf(stderr, "warning: %s does not exist, skipping\n", dir.c_str());
            continue;
        }
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            // Accept any regular file: real corpora (SpamAssassin, Enron)
            // name messages by hash with no meaningful extension, so
            // filtering on one silently skips the entire corpus. Only
            // obvious non-messages are excluded.
            const auto ext = entry.path().extension().string();
            const auto name = entry.path().filename().string();
            if (ext == ".bz2" || ext == ".gz" || ext == ".zip" || ext == ".tar" ||
                name == "cmds" || name == ".DS_Store") {
                continue;
            }
            const std::string raw = read_file(entry.path());
            if (mbox) {
                for (const std::string& msg_text : split_mbox(raw)) {
                    process_one_message(msg_text, s);
                }
            } else {
                process_one_message(raw, s);
            }
        }
    }

    if (s.total == 0) {
        std::fprintf(stderr, "error: no messages found\n");
        return 2;
    }

    const double success = static_cast<double>(s.parsed) / static_cast<double>(s.total);
    std::printf("messages:        %zu\n", s.total);
    std::printf("parsed:          %zu (%.2f%%)\n", s.parsed, 100.0 * success);
    std::printf("rejected(policy):%zu\n", s.rejected);
    std::printf("parse errors:    %zu\n", s.threw);
    if (s.text_parts > 0) {
        std::printf("text decoded:    %zu/%zu (%.2f%%)\n", s.text_decoded, s.text_parts,
                    100.0 * static_cast<double>(s.text_decoded) / static_cast<double>(s.text_parts));
    }
    if (!quiet && !s.anomalies.empty()) {
        std::printf("anomalies:\n");
        for (const auto& [name, count] : s.anomalies) {
            std::printf("  %-32s %zu\n", name.c_str(), count);
        }
    }

    if (success < min_success) {
        std::fprintf(stderr, "FAIL: success rate %.4f < required %.4f\n", success, min_success);
        return 1;
    }
    return 0;
}
