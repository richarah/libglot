#pragma once

#include "../ast/node.h"
#include "../dialect/traits.h"
#include <sstream>
#include <string>
#include <concepts>

namespace libglot {

/// ============================================================================
/// Generic Generator - Template-based code generation with dialect support
/// ============================================================================
///
/// Zero-cost abstraction for domain-specific code generation (SQL, MIME, etc.)
///
/// PERFORMANCE:
/// - All polymorphism resolved at compile-time via CRTP
/// - No virtual function calls
/// - Stream-based output for efficient string concatenation
///
/// DESIGN:
/// - Template parameter: Spec (must provide AstNodeType and DialectTraits)
/// - CRTP parameter: Derived (domain-specific generator)
/// - Core output and formatting infrastructure in base class
/// - Domain-specific code generation in derived class
///
/// USAGE:
///   See example at bottom of file for SQL generator implementation.
///
/// CUSTOMIZATION POINTS (implement in derived class):
///   - visit(AstNodeType*) - Main visitor dispatch
///   - visit_* methods for each node type
/// ============================================================================

template<typename Spec, typename Derived>
    requires requires {
        typename Spec::AstNodeType;
        typename Spec::NodeKind;
        typename Spec::DialectTraitsType;
        requires AstNode<typename Spec::AstNodeType>;
        requires DialectTraits<typename Spec::DialectTraitsType>;
    }
class GeneratorBase {
public:
    // ========================================================================
    // Type Aliases
    // ========================================================================

    using AstNodeType = typename Spec::AstNodeType;
    using NodeKind = typename Spec::NodeKind;
    using DialectTraitsType = typename Spec::DialectTraitsType;
    using DialectId = typename DialectTraitsType::DialectId;
    using Features = typename DialectTraitsType::Features;

    // ========================================================================
    // Generator Options
    // ========================================================================

    struct Options {
        bool pretty;                  ///< Enable pretty-printing with indentation
        int indent_width;             ///< Number of spaces per indent level
        bool trailing_comma;          ///< Put commas at end of lines

        Options() : pretty(false), indent_width(2), trailing_comma(false) {}
    };

    // ========================================================================
    // Construction
    // ========================================================================

    explicit GeneratorBase(DialectId dialect, const Options& opts = Options{})
        : dialect_(dialect)
        , features_(DialectTraitsType::get_features(dialect))
        , options_(opts)
        , output_()
        , indent_level_(0)
    {}

    // ========================================================================
    // Public API
    // ========================================================================

    /// Generate code from AST
    std::string generate(AstNodeType* root) {
        derived().visit(root);
        return output_.str();
    }

    /// Get current output
    [[nodiscard]] std::string current_output() const {
        return output_.str();
    }

    /// Reset generator state
    void reset() {
        output_.str("");
        output_.clear();
        indent_level_ = 0;
    }

    // ========================================================================
    // Protected Helpers (for derived classes)
    // ========================================================================

protected:
    /// CRTP: Get reference to derived class
    [[nodiscard]] Derived& derived() noexcept {
        return static_cast<Derived&>(*this);
    }

    [[nodiscard]] const Derived& derived() const noexcept {
        return static_cast<const Derived&>(*this);
    }

    // ========================================================================
    // Output Primitives
    // ========================================================================

    /// Write text to output
    void write(std::string_view text) {
        output_ << text;
    }

    /// Write character to output
    void write(char c) {
        output_ << c;
    }

    /// Write number to output
    template<typename T>
        requires std::is_arithmetic_v<T>
    void write(T value) {
        output_ << value;
    }

    // ========================================================================
    // Formatting Primitives
    // ========================================================================

    /// Write newline and indent if pretty-printing
    void newline() {
        if (options_.pretty) {
            output_ << '\n';
            output_ << std::string(indent_level_ * options_.indent_width, ' ');
        }
    }

    /// Write space
    void space() {
        output_ << ' ';
    }

    /// Write space or newline based on pretty mode
    void space_or_newline() {
        if (options_.pretty) {
            newline();
        } else {
            space();
        }
    }

    /// Increase indent level
    void indent() {
        if (options_.pretty) {
            ++indent_level_;
        }
    }

    /// Decrease indent level
    void dedent() {
        if (options_.pretty && indent_level_ > 0) {
            --indent_level_;
        }
    }

    // ========================================================================
    // List Formatting Helpers
    // ========================================================================

    /// Generate comma-separated list
    ///
    /// @param items Vector of items to generate
    /// @param generator Function to generate each item
    template<typename T, typename GenerateFunc>
    void write_list(
        const std::vector<T>& items,
        GenerateFunc generator,
        const char* separator = ", "
    ) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                write(separator);
            }
            generator(items[i]);
        }
    }

    /// Generate comma-separated list with optional line breaks
    template<typename T, typename GenerateFunc>
    void write_list_multiline(
        const std::vector<T>& items,
        GenerateFunc generator,
        bool force_multiline = false
    ) {
        const bool multiline = options_.pretty && (force_multiline || items.size() > 3);

        if (multiline) {
            indent();
        }

        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                if (options_.trailing_comma) {
                    write(',');
                    if (multiline) {
                        newline();
                    } else {
                        space();
                    }
                } else {
                    if (multiline) {
                        newline();
                    } else {
                        space();
                    }
                    write(',');
                    space();
                }
            } else if (multiline) {
                newline();
            }

            generator(items[i]);
        }

        if (multiline) {
            dedent();
            newline();
        }
    }

    /// Generate parenthesized list: (item1, item2, item3)
    template<typename T, typename GenerateFunc>
    void write_paren_list(
        const std::vector<T>& items,
        GenerateFunc generator,
        const char* separator = ", "
    ) {
        write('(');
        write_list(items, generator, separator);
        write(')');
    }

    // ========================================================================
    // Identifier and Literal Helpers
    // ========================================================================

    /// Quote identifier using dialect-specific rules
    void quote_identifier(std::string_view ident) {
        // Let derived class override with dialect-specific logic
        derived().write_identifier(ident);
    }

    /// Write string literal with dialect-specific quoting
    void quote_string(std::string_view str) {
        // Let derived class override with dialect-specific logic
        derived().write_string_literal(str);
    }

    /// Default identifier writing (no quoting)
    void write_identifier(std::string_view ident) {
        write(ident);
    }

    /// Default string literal writing (single quotes)
    void write_string_literal(std::string_view str) {
        write('\'');
        // Escape quotes by doubling
        for (char c : str) {
            write(c);
            if (c == '\'') {
                write('\'');  // Double quote
            }
        }
        write('\'');
    }

    // ========================================================================
    // Keyword Helpers
    // ========================================================================

    /// Write keyword (uppercase in standard mode, lowercase in pretty mode)
    void keyword(std::string_view kw) {
        // By default, write as-is. Derived class can override for case conversion.
        write(kw);
    }

    // ========================================================================
    // Dialect and Feature Access
    // ========================================================================

    [[nodiscard]] DialectId dialect() const noexcept {
        return dialect_;
    }

    [[nodiscard]] const Features& features() const noexcept {
        return features_;
    }

    [[nodiscard]] const Options& options() const noexcept {
        return options_;
    }

    // ========================================================================
    // Member Variables
    // ========================================================================

    DialectId dialect_;
    const Features& features_;
    Options options_;
    std::ostringstream output_;
    int indent_level_;
};

// ============================================================================
/// Simple Generator (no dialect support, minimal formatting)
/// ============================================================================

template<typename Spec>
    requires requires {
        typename Spec::AstNodeType;
        typename Spec::NodeKind;
        requires AstNode<typename Spec::AstNodeType>;
    }
class SimpleGenerator {
public:
    using AstNodeType = typename Spec::AstNodeType;
    using NodeKind = typename Spec::NodeKind;

    struct Options {
        bool pretty = false;
        int indent_width = 2;
    };

    explicit SimpleGenerator(const Options& opts = Options{})
        : options_(opts)
        , output_()
        , indent_level_(0)
    {}

    /// Generate code from AST (must be implemented by derived class)
    std::string generate(AstNodeType* root) {
        visit(root);
        return output_.str();
    }

    /// Visit AST node (must be implemented by derived class or CRTP derived)
    void visit(AstNodeType* node) {
        // Derived class must implement this
        static_assert(sizeof(Spec) == 0, "SimpleGenerator requires derived class to implement visit()");
    }

protected:
    void write(std::string_view text) { output_ << text; }
    void write(char c) { output_ << c; }
    void space() { output_ << ' '; }
    void newline() {
        if (options_.pretty) {
            output_ << '\n' << std::string(indent_level_ * options_.indent_width, ' ');
        }
    }
    void indent() { if (options_.pretty) ++indent_level_; }
    void dedent() { if (options_.pretty && indent_level_ > 0) --indent_level_; }

    Options options_;
    std::ostringstream output_;
    int indent_level_;
};

// ============================================================================
/// Example: SQL Generator (for documentation)
/// ============================================================================

#if 0  // Example only, not compiled

// Assume we have SQLGrammarSpec with AstNodeType, NodeKind, and DialectTraitsType
struct SQLGeneratorSpec {
    using AstNodeType = SQLNode;
    using NodeKind = SQLNodeKind;
    using DialectTraitsType = SQLDialectTraits;
};

class SQLGenerator : public GeneratorBase<SQLGeneratorSpec, SQLGenerator> {
public:
    using Base = GeneratorBase<SQLGeneratorSpec, SQLGenerator>;
    using Base::Base;

    // Main visitor dispatch
    void visit(AstNodeType* node) {
        if (!node) return;

        switch (node->type) {
            case NodeKind::SELECT:
                visit_select(static_cast<SelectStmt*>(node));
                break;
            case NodeKind::INSERT:
                visit_insert(static_cast<InsertStmt*>(node));
                break;
            case NodeKind::BINARY_OP:
                visit_binary_op(static_cast<BinaryOp*>(node));
                break;
            // ... more cases
            default:
                break;
        }
    }

    // Domain-specific visit methods
    void visit_select(SelectStmt* stmt) {
        keyword("SELECT");
        space();

        if (stmt->distinct) {
            keyword("DISTINCT");
            space();
        }

        // Columns
        write_list(stmt->columns, [this](auto* col) {
            visit(col);
        });

        // FROM clause
        if (stmt->from) {
            space_or_newline();
            keyword("FROM");
            space();
            visit(stmt->from);
        }

        // ... more clauses
    }

    void visit_insert(InsertStmt* stmt) {
        keyword("INSERT INTO");
        space();
        visit(stmt->table);

        // Columns
        if (!stmt->columns.empty()) {
            space();
            write_paren_list(stmt->columns, [this](const std::string& col) {
                quote_identifier(col);
            });
        }

        // VALUES
        space();
        keyword("VALUES");
        space();
        // ... generate values
    }

    void visit_binary_op(BinaryOp* op) {
        visit(op->left);
        space();
        write(operator_string(op->op));
        space();
        visit(op->right);
    }

private:
    std::string operator_string(BinaryOpKind op) {
        // ... convert operator to string
        return "+";
    }
};

#endif  // Example

} // namespace libglot
