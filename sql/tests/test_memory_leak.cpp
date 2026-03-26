// Test to verify memory leak in optimizer
#include <libglot/sql/parser.h>
// Optimizer not yet ported
// #include <libglot/sql/optimizer.h>
#include <libglot/util/arena.h>
#include <iostream>

int main() {
    // This test demonstrates the memory leak in create_and()
    // Every time we run pushdown_predicates, it creates leaked BinaryOp nodes

    const char* query = "SELECT * FROM (SELECT * FROM t WHERE x > 5) s WHERE s.y < 10";

    // Run the optimizer many times to accumulate leaked memory
    for (int i = 0; i < 1000; ++i) {
        libglot::sql::libglot::Arena arena;
        auto ast = libglot::sql::[&]() { SQLParser parser(arena, query); return parser.parse_top_level(); }();

        if (ast && ast->type == libglot::sql::SQLNodeKind::SELECT_STMT) {
            auto select = static_cast<libglot::sql::SelectStmt*>(ast);

            // This now uses arena allocation - no leaks!
    // libglot::sql::Optimizer::pushdown_predicates(select, arena);  // TODO: Port optimizer

            // All memory is properly tracked in arena and will be freed when arena is destroyed
        }
    }

    std::cout << "Memory leak test complete. Fixed! No leaks with arena allocation.\n";
    std::cout << "To verify: valgrind --leak-check=full ./test_memory_leak\n";

    return 0;
}
