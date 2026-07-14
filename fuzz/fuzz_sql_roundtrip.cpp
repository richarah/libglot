// libFuzzer harness: SQL parse -> generate -> re-parse.
//
// Anything the parser accepts, the generator must render into SQL that the
// parser accepts again (the transpiler contract). A ParseError on the
// GENERATED text is a real bug, so it is not caught.

#include <libglot/sql/generator.h>
#include <libglot/sql/parser.h>
#include <libglot/util/arena.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using libglot::sql::SQLDialect;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }

    const auto parse_dialect = static_cast<SQLDialect>(
        data[0] % static_cast<uint8_t>(SQLDialect::COUNT));
    const auto gen_dialect = static_cast<SQLDialect>(
        data[1] % static_cast<uint8_t>(SQLDialect::COUNT));
    std::string_view source(reinterpret_cast<const char*>(data + 2), size - 2);

    libglot::Arena arena;
    libglot::sql::SQLNode* ast = nullptr;
    try {
        libglot::sql::SQLParser parser(arena, source, parse_dialect);
        ast = parser.parse_top_level();
    } catch (const libglot::ParseError&) {
        return 0; // Malformed input: fine.
    } catch (const libglot::MultipleParseErrors&) {
        return 0;
    }
    if (ast == nullptr) {
        return 0;
    }

    libglot::sql::SQLGenerator gen(gen_dialect);
    const std::string emitted = gen.generate(ast);

    // Generated SQL must re-parse in the dialect it was generated for.
    libglot::Arena arena2;
    libglot::sql::SQLParser reparser(arena2, emitted, gen_dialect);
    (void)reparser.parse_top_level(); // A throw here is a finding.
    return 0;
}
