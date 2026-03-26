#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace libglot {

/// Detailed parse error with location information
struct ParseErrorDetail {
    std::string message;
    size_t position;
    size_t line;
    size_t column;
    std::string context;  // Surrounding source code snippet

    ParseErrorDetail(std::string msg, size_t pos, size_t ln = 0, size_t col = 0, std::string ctx = "")
        : message(std::move(msg)), position(pos), line(ln), column(col), context(std::move(ctx)) {}

    std::string format() const {
        std::string result = "Parse error at position " + std::to_string(position);
        if (line > 0) {
            result += " (line " + std::to_string(line) + ", column " + std::to_string(column) + ")";
        }
        result += ": " + message;
        if (!context.empty()) {
            result += "\n  " + context;
        }
        return result;
    }
};

/// Error recovery modes
enum class ErrorRecoveryMode {
    /// Throw immediately on first error (default, backward compatible)
    THROW_ON_ERROR,
    /// Collect all errors and continue parsing (error recovery mode)
    COLLECT_ERRORS,
    /// Collect errors but also return partial AST (best effort)
    BEST_EFFORT
};

/// Error recovery synchronization points
enum class SyncPoint {
    NONE,
    SEMICOLON,      // ;
    STATEMENT_KW,   // SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, etc.
    COMMA,          // , (for lists)
    RPAREN,         // ) (for expressions)
    EOF_TOKEN       // End of input
};

/// Error collector for multi-error reporting
class ErrorCollector {
public:
    ErrorCollector(ErrorRecoveryMode mode = ErrorRecoveryMode::THROW_ON_ERROR)
        : mode_(mode), max_errors_(100) {}

    /// Add an error to the collection
    void add_error(std::string message, size_t position, size_t line = 0, size_t column = 0, std::string context = "") {
        if (errors_.size() >= max_errors_) {
            // Already at max, don't add more
            if (errors_.size() == max_errors_) {
                errors_.push_back(ParseErrorDetail("Too many errors, stopping error collection", position, line, column));
            }
            return;
        }
        errors_.emplace_back(std::move(message), position, line, column, std::move(context));
    }

    /// Check if any errors were collected
    bool has_errors() const {
        return !errors_.empty();
    }

    /// Get number of errors
    size_t error_count() const {
        return errors_.size();
    }

    /// Get all errors
    const std::vector<ParseErrorDetail>& get_errors() const {
        return errors_;
    }

    /// Get formatted error report
    std::string format_errors() const {
        if (errors_.empty()) {
            return "No errors";
        }

        std::string result = "Found " + std::to_string(errors_.size()) + " error(s):\n";
        for (size_t i = 0; i < errors_.size(); ++i) {
            result += "\n" + std::to_string(i + 1) + ". " + errors_[i].format();
        }
        return result;
    }

    /// Clear all errors
    void clear() {
        errors_.clear();
    }

    /// Get error recovery mode
    ErrorRecoveryMode get_mode() const {
        return mode_;
    }

    /// Set maximum number of errors to collect
    void set_max_errors(size_t max) {
        max_errors_ = max;
    }

private:
    ErrorRecoveryMode mode_;
    std::vector<ParseErrorDetail> errors_;
    size_t max_errors_;
};

/// Exception thrown when collecting multiple errors
class MultipleParseErrors : public std::runtime_error {
public:
    explicit MultipleParseErrors(const ErrorCollector& collector)
        : std::runtime_error(collector.format_errors())
        , errors_(collector.get_errors()) {}

    const std::vector<ParseErrorDetail>& get_errors() const {
        return errors_;
    }

private:
    std::vector<ParseErrorDetail> errors_;
};

/// Helper for panic mode error recovery
class PanicModeRecovery {
public:
    /// Find next synchronization point in token stream
    template<typename TokenIterator>
    static TokenIterator synchronize(TokenIterator current, TokenIterator end, SyncPoint sync_to) {
        switch (sync_to) {
            case SyncPoint::SEMICOLON:
                // Skip until we find ; or statement keyword
                while (current != end && !is_sync_point(*current)) {
                    ++current;
                }
                break;

            case SyncPoint::STATEMENT_KW:
                // Skip until we find statement keyword
                while (current != end && !is_statement_keyword(*current)) {
                    ++current;
                }
                break;

            case SyncPoint::COMMA:
                // Skip until we find comma or higher-level sync point
                while (current != end && !is_list_separator(*current)) {
                    ++current;
                }
                break;

            case SyncPoint::RPAREN:
                // Skip until matching right paren
                while (current != end && !is_rparen(*current)) {
                    ++current;
                }
                break;

            case SyncPoint::EOF_TOKEN:
                // Go to end
                current = end;
                break;

            case SyncPoint::NONE:
            default:
                break;
        }

        return current;
    }

    /// Check if token is a synchronization point
    template<typename Token>
    static bool is_sync_point(const Token&) {
        // In real implementation, check token type
        // For now, this is a placeholder
        return false;
    }

    /// Check if token is a statement keyword
    template<typename Token>
    static bool is_statement_keyword(const Token&) {
        // SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, etc.
        return false;
    }

    /// Check if token is a list separator
    template<typename Token>
    static bool is_list_separator(const Token&) {
        // Comma or closing paren/bracket
        return false;
    }

    /// Check if token is right paren
    template<typename Token>
    static bool is_rparen(const Token&) {
        return false;
    }
};

/// RAII guard for error recovery context
class ErrorRecoveryGuard {
public:
    ErrorRecoveryGuard(ErrorCollector& collector, const char* context_name)
        : collector_(collector)
        , context_name_(context_name)
        , start_error_count_(collector.error_count()) {}

    ~ErrorRecoveryGuard() {
        // Could log recovery information if errors were added
        if (collector_.error_count() > start_error_count_) {
            // Recovery occurred in this context
        }
    }

    /// Check if this context added errors
    bool added_errors() const {
        return collector_.error_count() > start_error_count_;
    }

private:
    ErrorCollector& collector_;
    [[maybe_unused]] const char* context_name_;
    size_t start_error_count_;
};

} // namespace libglot
