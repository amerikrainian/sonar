#include "sonar/parser.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace sonar {

namespace {
struct TokenTypeHash {
    std::size_t operator()(TokenType type) const noexcept {
        return static_cast<std::size_t>(type);
    }
};

}  // namespace

const Parser::PrefixParselet* Parser::find_prefix_rule(TokenType type) {
    using PrefixRuleMap = std::unordered_map<TokenType, PrefixParselet, TokenTypeHash>;

    static const PrefixRuleMap rules = []() {
        PrefixRuleMap map;
        map.emplace(TokenType::Number, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_number(std::move(token));
                    }});
        map.emplace(TokenType::String, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_string(std::move(token));
                    }});
        map.emplace(TokenType::True, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_boolean(std::move(token));
                    }});
        map.emplace(TokenType::False, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_boolean(std::move(token));
                    }});
        map.emplace(TokenType::Minus, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_prefix_operator(std::move(token));
                    }});
        map.emplace(TokenType::LeftParen, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_grouping(std::move(token));
                    }});
        map.emplace(TokenType::Identifier, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_identifier(std::move(token));
                    }});
        map.emplace(TokenType::Fn, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_function_literal(std::move(token));
                    }});
        map.emplace(TokenType::LeftBrace, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_block(std::move(token));
                    }});
        map.emplace(TokenType::If, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_if(std::move(token));
                    }});
        map.emplace(TokenType::While, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_while(std::move(token));
                    }});
        map.emplace(TokenType::For, PrefixParselet{[](Parser& parser, Token token) {
                        return parser.parse_for(std::move(token));
                    }});
        return map;
    }();

    auto it = rules.find(type);
    if (it == rules.end()) {
        return nullptr;
    }
    return &it->second;
}

const Parser::InfixRule* Parser::find_infix_rule(TokenType type) {
    using InfixRuleMap = std::unordered_map<TokenType, InfixRule, TokenTypeHash>;

    static const InfixRuleMap rules = []() {
        InfixRuleMap map;
        auto make_rule = [](Precedence precedence, bool right_associative) {
            return InfixRule{precedence,
                             right_associative,
                             InfixParselet{[](Parser& parser, ExpressionPtr left, Token op, Precedence precedence_value,
                                              bool is_right_associative) {
                                 return parser.parse_binary_operator(std::move(left), std::move(op), precedence_value, is_right_associative);
                             }}};
        };

        map.emplace(TokenType::Equals,
                    InfixRule{
                        Precedence::Assignment,
                        true,
                        InfixParselet{
                            [](Parser& parser, ExpressionPtr left, Token op,
                               Precedence precedence_value, bool /*unused*/) {
                                return parser.parse_assignment(std::move(left), std::move(op), precedence_value);
                            }}});
        map.emplace(TokenType::OrOr, make_rule(Precedence::LogicalOr, false));
        map.emplace(TokenType::AndAnd, make_rule(Precedence::LogicalAnd, false));
        map.emplace(TokenType::Pipe, make_rule(Precedence::BitwiseOr, false));
        map.emplace(TokenType::Ampersand, make_rule(Precedence::BitwiseAnd, false));
        map.emplace(TokenType::Plus, make_rule(Precedence::Sum, false));
        map.emplace(TokenType::Minus, make_rule(Precedence::Sum, false));
        map.emplace(TokenType::Star, make_rule(Precedence::Product, false));
        map.emplace(TokenType::Slash, make_rule(Precedence::Product, false));
        return map;
    }();

    auto it = rules.find(type);
    if (it == rules.end()) {
        return nullptr;
    }
    return &it->second;
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

ExpressionPtr Parser::parse_expression(Precedence precedence_floor) {
    if (is_at_end()) {
        const Token& end_token = peek();
        throw make_error("Unexpected end of input while parsing expression", end_token.span, true);
    }

    Token token = advance();
    const PrefixParselet* prefix_rule = find_prefix_rule(token.type);

    if (!prefix_rule) {
        if (token.type == TokenType::Let) {
            throw make_error("Unexpected 'let' while parsing expression", token.span, false);
        }
        if (token.type == TokenType::End) {
            throw make_error("Unexpected end of input while parsing expression", token.span, true);
        }
        throw make_error("Unexpected token '" + token.lexeme + "' while parsing expression", token.span, false);
    }

    auto left = (*prefix_rule)(*this, std::move(token));

    while (!is_at_end()) {
        const Token& next = peek();
        const InfixRule* infix_rule = find_infix_rule(next.type);
        if (!infix_rule || static_cast<int>(infix_rule->precedence) < static_cast<int>(precedence_floor)) {
            break;
        }

        Token op = advance();
        left = infix_rule->parselet(*this, std::move(left), std::move(op), infix_rule->precedence, infix_rule->right_associative);
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
            if (check(TokenType::Semicolon)) {
                throw make_error("Unexpected ';' after function definition", peek().span, false);
            }
            sequence.statements.push_back(std::move(stmt));
            continue;
        }

        auto expr = parse_expression();

        if (match(TokenType::Semicolon)) {
            sequence.statements.push_back(make_expression_statement(std::move(expr)));
            continue;
        }

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

ExpressionPtr Parser::parse_boolean(Token literal) {
    Expression::Boolean node{literal.type == TokenType::True, literal.span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_string(Token literal) {
    Expression::String node{std::move(literal.lexeme), literal.span};
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
    auto right = parse_expression(Precedence::Prefix);
    SourceSpan right_span = right->span;
    SourceSpan span{op.span.start, right_span.end};
    Expression::Prefix node{op.type, op.span, std::move(right), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_binary_operator(ExpressionPtr left, Token op, Precedence operator_precedence, bool right_associative) {
    int precedence_offset = static_cast<int>(operator_precedence) + (right_associative ? 0 : 1);
    auto next_precedence = static_cast<Precedence>(precedence_offset);
    auto right = parse_expression(next_precedence);
    SourceSpan left_span = left->span;
    SourceSpan right_span = right->span;
    SourceSpan span{left_span.start, right_span.end};
    Expression::Infix node{op.type, op.span, std::move(left), std::move(right), span};
    return std::make_unique<Expression>(std::move(node));
}

ExpressionPtr Parser::parse_assignment(ExpressionPtr left, Token op, Precedence precedence) {
    auto* var = std::get_if<Expression::Variable>(&left->node);
    if (!var) {
        throw make_error("Left-hand side of assignment must be a variable", op.span, false);
    }
    auto right = parse_expression(precedence);
    SourceSpan span{left->span.start, right->span.end};
    Expression::Assign node{var->name, var->name_span, std::move(right), span};
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
