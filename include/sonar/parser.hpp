#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sonar/ast.hpp"
#include "sonar/token.hpp"

namespace sonar {

class ParseError : public std::runtime_error {
   public:
    ParseError(std::string message, bool incomplete, SourceSpan span, SourceLocation location, std::string source_name)
        : std::runtime_error(std::move(message)),
          incomplete_(incomplete),
          span_(span),
          location_(location),
          source_name_(std::move(source_name)) {}

    bool incomplete() const noexcept { return incomplete_; }

    SourceSpan span() const noexcept { return span_; }

    SourceLocation location() const noexcept { return location_; }

    const std::string& source_name() const noexcept { return source_name_; }

   private:
    bool incomplete_{false};
    SourceSpan span_{};
    SourceLocation location_{};
    std::string source_name_{};
};

class Parser {
   public:
    Parser(std::vector<Token> tokens, std::vector<std::size_t> line_offsets, std::string source_name);

    ExpressionPtr parse();

   private:
    ExpressionPtr parse_expression(int precedence = 0);
    ExpressionPtr parse_prefix();
    ExpressionPtr parse_infix(ExpressionPtr left, Token op);

    bool match(TokenType type);
    const Token& advance();
    bool check(TokenType type) const;
    bool is_at_end() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& consume(TokenType type, const std::string& message);

    int precedence(TokenType type) const;

    ExpressionPtr parse_number();
    ExpressionPtr parse_grouping();
    ExpressionPtr parse_prefix_operator();
    ExpressionPtr parse_binary_operator(ExpressionPtr left, Token op);

    ParseError make_error(const std::string& message, SourceSpan span, bool incomplete) const;
    SourceLocation location_for(std::size_t offset) const;

    std::vector<Token> tokens_;
    std::size_t current_{0};
    std::vector<std::size_t> line_offsets_;
    std::string source_name_;
};

}  // namespace sonar
