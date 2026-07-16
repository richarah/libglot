// MIME corpus runner: parse every *.eml file under one or more directories
// through the full pipeline and report aggregate statistics. Exits non-zero
// when the parse success rate falls below --min-success (default 1.0), so it
// doubles as a CI gate and a benchmark harness.
//
//   mime_corpus [--min-success 0.95] [--quiet] DIR [DIR...]

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

} // namespace

int main(int argc, char** argv) {
    double min_success = 1.0;
    bool quiet = false;
    std::vector<fs::path> dirs;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--min-success" && i + 1 < argc) {
            min_success = std::atof(argv[++i]);
        } else if (a == "--quiet") {
            quiet = true;
        } else {
            dirs.emplace_back(a);
        }
    }
    if (dirs.empty()) {
        std::fprintf(stderr, "usage: mime_corpus [--min-success F] [--quiet] DIR...\n");
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
            ++s.total;
            const std::string raw = read_file(entry.path());
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
