// libFuzzer harness: SQL parser robustness.
//
// Any input may be rejected with ParseError; nothing may crash, leak, or
// trip ASan/UBSan. The first input byte selects the dialect so dialect-
// specific lexing (TokenizerConfig) is fuzzed too.

#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

using libglot::sql::SQLDialect;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    const auto dialect = static_cast<SQLDialect>(
        data[0] % static_cast<uint8_t>(SQLDialect::COUNT));
    std::string_view source(reinterpret_cast<const char*>(data + 1), size - 1);

    libglot::Arena arena;
    try {
        libglot::sql::SQLParser parser(arena, source, dialect);
        (void)parser.parse_top_level();
    } catch (const libglot::ParseError&) {
        // Expected for malformed input.
    } catch (const libglot::MultipleParseErrors&) {
        // Expected for malformed input.
    }
    return 0;
}
