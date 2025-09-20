#include "sonar/parser.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace sonar {

namespace {
constexpr int kPrefixPrecedence = 30;
}

const Parser::PrefixRule* Parser::find_prefix_rule(TokenType type) {
    static constexpr PrefixRule kRules[] = {
        {TokenType::Number, &Parser::parse_number},
        {TokenType::Minus, &Parser::parse_prefix_operator},
        {TokenType::LeftParen, &Parser::parse_grouping},
        {TokenType::Identifier, &Parser::parse_identifier},
        {TokenType::Fn, &Parser::parse_function_literal},
        {TokenType::LeftBrace, &Parser::parse_block},
        {TokenType::If, &Parser::parse_if},
        {TokenType::While, &Parser::parse_while},
        {TokenType::For, &Parser::parse_for},
    };

    for (const auto& rule : kRules) {
        if (rule.type == type) {
            return &rule;
        }
    }
    return nullptr;
}

const Parser::InfixRule* Parser::find_infix_rule(TokenType type) {
    static constexpr InfixRule kRules[] = {
        {TokenType::Equals, 5, true, &Parser::parse_binary_operator},
        {TokenType::Plus, 10, false, &Parser::parse_binary_operator},
        {TokenType::Minus, 10, false, &Parser::parse_binary_operator},
        {TokenType::Star, 20, false, &Parser::parse_binary_operator},
        {TokenType::Slash, 20, false, &Parser::parse_binary_operator},
    };

    for (const auto& rule : kRules) {
        if (rule.type == type) {
            return &rule;
        }
    }
    return nullptr;
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
    auto sequence = parse_sequence(TokenType::End);
    consume(TokenType::End, "Expected end of input");

    if (sequence.statements.empty()) {
        if (!sequence.value) {
            const Token& end_token = tokens_.back();
            Expression::Unit node{end_token.span};
            return std::make_unique<Expression>(std::move(node));
        }
        return std::move(sequence.value);
    }

    SourceSpan span{sequence.statements.front()->span.start,
                    sequence.value ? sequence.value->span.end : sequence.statements.back()->span.end};
    Expression::Block node{std::move(sequence.statements), std::move(sequence.value), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_expression(int precedence_floor) {
    if (is_at_end()) {
        const Token& end_token = peek();
        throw make_error("Unexpected end of input while parsing expression", end_token.span, true);
    }

    Token token = advance();
    const PrefixRule* prefix_rule = find_prefix_rule(token.type);

    if (!prefix_rule) {
        if (token.type == TokenType::Let) {
            throw make_error("Unexpected 'let' while parsing expression", token.span, false);
        }
        if (token.type == TokenType::End) {
            throw make_error("Unexpected end of input while parsing expression", token.span, true);
        }
        throw make_error("Unexpected token '" + token.lexeme + "' while parsing expression", token.span, false);
    }

    auto left = (this->*prefix_rule->parselet)(std::move(token));

    while (!is_at_end()) {
        const Token& next = peek();
        const InfixRule* infix_rule = find_infix_rule(next.type);
        if (!infix_rule || infix_rule->precedence < precedence_floor) {
            break;
        }

        Token op = advance();
        left = (this->*infix_rule->parselet)(std::move(left), std::move(op), infix_rule->precedence, infix_rule->right_associative);
    }

    return left;
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
        return previous();
    }
    return tokens_.back();
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

const Token& Parser::peek(std::size_t offset) const {
    std::size_t index = std::min(current_ + offset, tokens_.size() - 1);
    return tokens_.at(index);
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

Parser::StatementSequence Parser::parse_sequence(TokenType terminator) {
    StatementSequence sequence;

    while (!check(terminator) && !is_at_end()) {
        // Allow empty statements (just ;)
        if (check(TokenType::Semicolon)) {
            advance();
            continue;
        }

        if (check(TokenType::Let)) {
            auto stmt = parse_let_statement();
            consume(TokenType::Semicolon, "Expected ';' after let statement");
            sequence.statements.push_back(std::move(stmt));
            continue;
        }

        if (check(TokenType::Fn) && peek(1).type == TokenType::Identifier) {
            auto stmt = parse_fn_statement();
            // no trailing semicolon allowed for fn
            if (check(TokenType::Semicolon)) {
                throw make_error("Unexpected ';' after function definition", peek().span, false);
            }
            sequence.statements.push_back(std::move(stmt));
            continue;
        }

        auto expr = parse_expression();

        if (match(TokenType::Semicolon)) {
            // any expression with semicolon is a statement
            sequence.statements.push_back(make_expression_statement(std::move(expr)));
            continue;
        }

        // no semicolon → must be the final value
        if (sequence.value) {
            throw make_error("Unexpected expression after final expression", expr->span, false);
        }

        sequence.value = std::move(expr);
        break;
    }

    return sequence;
}

StatementPtr Parser::parse_statement() {
    if (check(TokenType::Let)) {
        return parse_let_statement();
    }
    if (check(TokenType::Fn) && peek(1).type == TokenType::Identifier) {
        return parse_fn_statement();
    }

    auto expr = parse_expression();

    if (!match(TokenType::Semicolon)) {
        throw make_error("Expected ';' after expression statement", expr->span, false);
    }

    return make_expression_statement(std::move(expr));
}

StatementPtr Parser::make_expression_statement(ExpressionPtr expression) {
    SourceSpan span = expression ? expression->span : SourceSpan{};
    Statement::Expression node{std::move(expression), span};
    return std::make_unique<Statement>(std::move(node));
}

StatementPtr Parser::parse_let_statement() {
    Token let_token = consume(TokenType::Let, "Expected 'let'");
    const Token& name = consume(TokenType::Identifier, "Expected identifier after 'let'");
    consume(TokenType::Equals, "Expected '=' after identifier");
    auto initializer = parse_expression();
    SourceSpan span{let_token.span.start, initializer->span.end};
    Statement::Let node{name.lexeme, name.span, std::move(initializer), span};
    return std::make_unique<Statement>(std::move(node));
}

StatementPtr Parser::parse_fn_statement() {
    Token fn_token = consume(TokenType::Fn, "Expected 'fn'");
    const Token& name = consume(TokenType::Identifier, "Expected function name after 'fn'");
    auto function = parse_function_literal(fn_token);
    SourceSpan span{fn_token.span.start, function->span.end};
    Statement::Let node{name.lexeme, name.span, std::move(function), span};
    return std::make_unique<Statement>(std::move(node));
}

ExpressionPtr Parser::parse_number(Token literal) {
    Expression::Number node{literal.as_number(), literal.span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_grouping(Token open) {
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

ExpressionPtr Parser::parse_prefix_operator(Token op) {
    auto right = parse_expression(kPrefixPrecedence);
    SourceSpan right_span = right->span;
    SourceSpan span{op.span.start, right_span.end};
    Expression::Prefix node{op.type, op.span, std::move(right), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_binary_operator(ExpressionPtr left, Token op, int operator_precedence, bool right_associative) {
    if (op.type == TokenType::Equals) {
        auto* var = std::get_if<Expression::Variable>(&left->node);
        if (!var) {
            throw make_error("Left-hand side of assignment must be a variable", op.span, false);
        }

        auto right = parse_expression(operator_precedence);
        SourceSpan span{left->span.start, right->span.end};
        Expression::Assign node{var->name, var->name_span, std::move(right), span};
        return std::make_unique<Expression>(std::move(node));
    }

    int right_binding_power = operator_precedence + (right_associative ? 0 : 1);
    auto right = parse_expression(right_binding_power);
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

ExpressionPtr Parser::parse_block(Token open) {
    auto sequence = parse_sequence(TokenType::RightBrace);

    const Token& close = consume(TokenType::RightBrace, "Expected '}' after block");

    SourceSpan span{open.span.start, close.span.end};
    Expression::Block node{std::move(sequence.statements), std::move(sequence.value), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_if(Token if_token) {
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

ExpressionPtr Parser::parse_identifier(Token name) {
    Expression::Variable node{name.lexeme, name.span, name.span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_function_literal(Token fn_token) {
    consume(TokenType::LeftParen, "Expected '(' after 'fn'");

    std::vector<Expression::Function::Parameter> parameters;

    if (!check(TokenType::RightParen)) {
        while (true) {
            const Token& parameter = consume(TokenType::Identifier, "Expected parameter name");
            parameters.push_back(Expression::Function::Parameter{parameter.lexeme, parameter.span});
            if (!match(TokenType::Comma)) {
                break;
            }
        }
    }

    consume(TokenType::RightParen, "Expected ')' after parameter list");

    auto body = parse_expression();

    SourceSpan span{fn_token.span.start, body->span.end};
    Expression::Function node{std::move(parameters), std::move(body), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_while(Token while_token) {
    auto condition = parse_expression();
    auto body = parse_expression();
    SourceSpan span{while_token.span.start, body->span.end};
    Expression::While node{std::move(condition), std::move(body), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_for(Token for_token) {
    const Token& identifier = consume(TokenType::Identifier, "Expected identifier after 'for'");
    auto pattern = parse_identifier(identifier);
    consume(TokenType::In, "Expected 'in' after loop variable");

    auto iterable = parse_expression();
    auto body = parse_expression();

    SourceSpan span{for_token.span.start, body->span.end};
    Expression::For node{std::move(pattern), std::move(iterable), std::move(body), span};
    return std::make_unique<Expression>(std::move(node));
}

}  // namespace sonar
