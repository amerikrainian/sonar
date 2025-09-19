#include "sonar/parser.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sonar {

namespace {
constexpr int kPrefixPrecedence = 30;
}

Parser::Parser(std::vector<Token> tokens, std::vector<std::size_t> line_offsets, std::string source_name)
    : tokens_(std::move(tokens)),
      line_offsets_(std::move(line_offsets)),
      source_name_(std::move(source_name)) {
    if (line_offsets_.empty()) {
        line_offsets_.push_back(0);
    }
}

ExpressionPtr Parser::parse() {
    auto expression = parse_expression();
    consume(TokenType::End, "Expected end of input");
    return expression;
}

ExpressionPtr Parser::parse_expression(int precedence_floor) {
    auto left = parse_prefix();

    while (!is_at_end()) {
        const Token& next = peek();
        int next_precedence = precedence(next.type);
        if (next_precedence < precedence_floor) {
            break;
        }
        Token op = advance();
        left = parse_infix(std::move(left), std::move(op));
    }

    return left;
}

ExpressionPtr Parser::parse_prefix() {
    const Token& token = peek();
    switch (token.type) {
        case TokenType::Number:
            advance();
            return parse_number();
        case TokenType::Minus:
            advance();
            return parse_prefix_operator();
        case TokenType::LeftParen:
            return parse_grouping();
        case TokenType::End:
            throw make_error("Unexpected end of input while parsing expression", token.span, true);
        case TokenType::Let:
            return parse_let();
        case TokenType::Identifier:
            return parse_identifier();
        case TokenType::LeftBrace:
            return parse_block();
        case TokenType::If:
            return parse_if();
        default:
            throw make_error("Unexpected token '" + token.lexeme + "' while parsing expression", token.span, false);
    }
}

ExpressionPtr Parser::parse_infix(ExpressionPtr left, Token op) {
    switch (op.type) {
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Star:
        case TokenType::Slash:
        case TokenType::Equals:
            return parse_binary_operator(std::move(left), op);
        default:
            throw make_error("Unsupported infix operator: " + to_string(op.type), op.span, false);
    }
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        ++current_;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    return tokens_.at(current_).type == type;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::End;
}

const Token& Parser::peek() const {
    return tokens_.at(current_);
}

const Token& Parser::previous() const {
    return tokens_.at(current_ - 1);
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    SourceSpan error_span = is_at_end() ? tokens_.back().span : peek().span;
    throw make_error(message, error_span, is_at_end());
}

int Parser::precedence(TokenType type) const {
    switch (type) {
        case TokenType::Equals:
            return 5;
        case TokenType::Plus:
        case TokenType::Minus:
            return 10;
        case TokenType::Star:
        case TokenType::Slash:
            return 20;
        default:
            return -1;
    }
}

ExpressionPtr Parser::parse_number() {
    const Token& literal = previous();
    Expression::Number node{literal.as_number(), literal.span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_grouping() {
    Token open = consume(TokenType::LeftParen, "Expected '(' before expression");
    if (check(TokenType::RightParen)) {
        const Token& close = advance();
        SourceSpan span{open.span.start, close.span.end};
        Expression::Unit node{span};
        return std::make_unique<Expression>(std::move(node));
    }

    auto expression = parse_expression();
    const Token& close = consume(TokenType::RightParen, "Expected ')' after expression");
    SourceSpan span{open.span.start, close.span.end};
    Expression::Grouping node{std::move(expression), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_prefix_operator() {
    Token op = previous();
    auto right = parse_expression(kPrefixPrecedence);
    SourceSpan right_span = right->span;
    SourceSpan span{op.span.start, right_span.end};
    Expression::Prefix node{op.type, op.span, std::move(right), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_binary_operator(ExpressionPtr left, Token op) {
    int operator_precedence = precedence(op.type);

    if (op.type == TokenType::Equals) {
        auto* var = std::get_if<Expression::Variable>(&left->node);
        if (!var) {
            throw make_error("Left-hand side of assignment must be a variable", op.span, false);
        }

        auto right = parse_expression(operator_precedence);  // not +1 → right-assoc
        SourceSpan span{left->span.start, right->span.end};
        Expression::Assign node{var->name, var->name_span, std::move(right), span};
        return std::make_unique<Expression>(std::move(node));
    }

    auto right = parse_expression(operator_precedence + 1);
    SourceSpan left_span = left->span;
    SourceSpan right_span = right->span;
    SourceSpan span{left_span.start, right_span.end};
    Expression::Infix node{op.type, op.span, std::move(left), std::move(right), span};
    return std::make_unique<Expression>(std::move(node));
}

ParseError Parser::make_error(const std::string& message, SourceSpan span, bool incomplete) const {
    auto location = location_for(span.start);
    return ParseError(message, incomplete, span, location, source_name_);
}

SourceLocation Parser::location_for(std::size_t offset) const {
    auto it = std::upper_bound(line_offsets_.begin(), line_offsets_.end(), offset);
    std::size_t line_index = (it == line_offsets_.begin()) ? 0 : static_cast<std::size_t>(std::distance(line_offsets_.begin(), it) - 1);
    std::size_t line = line_index + 1;
    std::size_t column = offset - line_offsets_[line_index] + 1;
    return SourceLocation{line, column};
}

ExpressionPtr Parser::parse_block() {
    Token open = consume(TokenType::LeftBrace, "Expected '{' to start block");

    std::vector<ExpressionPtr> exprs;

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        exprs.push_back(parse_expression());
    }

    const Token& close = consume(TokenType::RightBrace, "Expected '}' after block");

    SourceSpan span{open.span.start, close.span.end};
    Expression::Block node{std::move(exprs), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_if() {
    Token if_token = consume(TokenType::If, "Expected 'if'");

    auto condition = parse_expression();

    auto then_branch = parse_expression();
    std::unique_ptr<Expression> else_branch;
    if (match(TokenType::Else)) {
        else_branch = parse_expression();
    }

    SourceSpan span{if_token.span.start,
                    (else_branch ? else_branch->span.end : then_branch->span.end)};

    Expression::If node{std::move(condition), std::move(then_branch), std::move(else_branch), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_let() {
    Token let_token = consume(TokenType::Let, "Expected 'let'");
    const Token& name = consume(TokenType::Identifier, "Expected identifier after 'let'");
    consume(TokenType::Equals, "Expected '=' after identifier");
    auto expr = parse_expression();
    SourceSpan span{name.span.start, expr->span.end};
    Expression::Let node{name.lexeme, name.span, std::move(expr), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_identifier() {
    Token name = advance();

    Expression::Variable node{name.lexeme, name.span, name.span};
    return std::make_unique<Expression>(std::move(node));
}

}  // namespace sonar
