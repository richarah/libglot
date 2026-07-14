// Parse a MIME message from stdin and print its structure, decoded text
// bodies, and any anomalies the parser recorded.
//
//   ./mime_inspect < message.eml

#include <libglot/mime/mime.h>
#include <libglot/util/arena.h>

#include <iostream>
#include <iterator>
#include <string>

namespace mime = libglot::mime;

namespace {

void print_part(const mime::Message& msg, int depth) {
    const std::string indent(static_cast<size_t>(depth) * 2, ' ');

    std::string content_type = "text/plain (implicit)";
    if (const auto* header = mime::find_header(msg, "Content-Type")) {
        content_type = std::string(header->value);
    }
    std::cout << indent << "- " << content_type << '\n';

    if (auto text = mime::decoded_body_utf8(msg)) {
        std::cout << indent << "  body (" << text->size() << " bytes UTF-8)\n";
    } else if (!msg.body.empty()) {
        std::cout << indent << "  body (" << msg.body.size()
                  << " raw bytes, not text-decodable)\n";
    }

    for (const auto* part : msg.parts) {
        if (part != nullptr) {
            print_part(*part, depth + 1);
        }
    }
}

} // namespace

int main() {
    const std::string raw(std::istreambuf_iterator<char>(std::cin), {});

    libglot::Arena arena;
    const mime::ParseResult result = mime::parse_message(arena, raw);

    if (result.rejected) {
        std::cout << "message REJECTED by anomaly policy\n";
    }
    if (result.message != nullptr) {
        print_part(*result.message, 0);
    }

    if (!result.report.records.empty()) {
        std::cout << "anomalies:\n";
        for (const auto& rec : result.report.records) {
            std::cout << "  - " << mime::anomaly_kind_name(rec.kind) << " ("
                      << mime::anomaly_severity_name(rec.severity) << ")\n";
        }
    }
    return result.rejected ? 1 : 0;
}
