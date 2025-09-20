#pragma once

#include <functional>
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
    enum class Precedence : int {
        Lowest = 0,
        Assignment,
        Sum,
        Product,
        Prefix,
    };

    using PrefixParselet = std::function<ExpressionPtr(Parser&, Token)>;
    using InfixParselet = std::function<ExpressionPtr(Parser&, ExpressionPtr, Token, Precedence, bool)>;

    struct InfixRule {
        Precedence precedence;
        bool right_associative;
        InfixParselet parselet;
    };

    static const PrefixParselet* find_prefix_rule(TokenType type);
    static const InfixRule* find_infix_rule(TokenType type);

    ExpressionPtr parse_expression(Precedence precedence = Precedence::Lowest);

    struct StatementSequence {
        std::vector<StatementPtr> statements;
        ExpressionPtr value;
    };

    StatementSequence parse_sequence(TokenType terminator);
    StatementPtr parse_statement();
    StatementPtr parse_let_statement();
    StatementPtr parse_fn_statement();
    StatementPtr make_expression_statement(ExpressionPtr expression);

    bool match(TokenType type);
    const Token& advance();
    bool check(TokenType type) const;
    bool is_at_end() const;
    const Token& peek() const;
    const Token& peek(std::size_t offset) const;
    const Token& previous() const;
    const Token& consume(TokenType type, const std::string& message);

    ExpressionPtr parse_number(Token literal);
    ExpressionPtr parse_grouping(Token open);
    ExpressionPtr parse_prefix_operator(Token op);
    ExpressionPtr parse_binary_operator(ExpressionPtr left, Token op, Precedence precedence, bool right_associative);
    ExpressionPtr parse_block(Token open);
    ExpressionPtr parse_if(Token if_token);
    ExpressionPtr parse_while(Token while_token);
    ExpressionPtr parse_for(Token for_token);
    ExpressionPtr parse_identifier(Token name);
    ExpressionPtr parse_function_literal(Token fn_token);

    ParseError make_error(const std::string& message, SourceSpan span, bool incomplete) const;
    SourceLocation location_for(std::size_t offset) const;

    std::vector<Token> tokens_;
    std::size_t current_{0};
    std::vector<std::size_t> line_offsets_;
    std::string source_name_;
};

}  // namespace sonar
