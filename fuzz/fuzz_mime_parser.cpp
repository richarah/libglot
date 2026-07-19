// libFuzzer harness: MIME parser robustness against hostile email input.
//
// Any input may be rejected with ParseError; nothing may crash, hang,
// recurse unboundedly, or trip ASan/UBSan. Uses the strict-ish standard
// limits so the DoS guards are part of the fuzzed surface.

#include <libglot/mime/parser_extended.h>
#include <libglot/util/arena.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view source(reinterpret_cast<const char*>(data), size);

    libglot::Arena arena;
    try {
        libglot::mime::MimeParserExtended parser(arena, source);
        (void)parser.parse_message_multipart();
    } catch (const libglot::ParseError&) {
        // Expected for malformed input.
    } catch (const libglot::MultipleParseErrors&) {
        // Expected for malformed input.
    }
    return 0;
}
