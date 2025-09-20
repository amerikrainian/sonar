#pragma once

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "sonar/token.hpp"

namespace sonar {

struct Expression;
struct Statement;

using ExpressionPtr = std::unique_ptr<Expression>;
using StatementPtr = std::unique_ptr<Statement>;

struct Expression {
    struct Number {
        double value;
        SourceSpan span;
    };

    struct Boolean {
        bool value;
        SourceSpan span;
    };

    struct String {
        std::string value;
        SourceSpan span;
    };

    struct Variable {
        std::string name;
        SourceSpan name_span;
        SourceSpan span;
    };

    struct Prefix {
        TokenType op;
        SourceSpan op_span;
        std::unique_ptr<Expression> right;
        SourceSpan span;
    };

    struct Infix {
        TokenType op;
        SourceSpan op_span;
        std::unique_ptr<Expression> left;
        std::unique_ptr<Expression> right;
        SourceSpan span;
    };

    struct Grouping {
        std::unique_ptr<Expression> expression;
        SourceSpan span;
    };

    struct Unit {
        SourceSpan span;
    };

    struct Assign {
        std::string name;
        SourceSpan name_span;
        std::unique_ptr<Expression> value;
        SourceSpan span;
    };

    struct Block {
        std::vector<StatementPtr> statements;
        ExpressionPtr value;
        SourceSpan span;
    };

    struct If {
        std::unique_ptr<Expression> condition;
        std::unique_ptr<Expression> then;
        std::unique_ptr<Expression> else_branch;
        SourceSpan span;
    };

    struct While {
        ExpressionPtr condition;
        ExpressionPtr body;
        SourceSpan span;
    };

    struct For {
        ExpressionPtr pattern;
        ExpressionPtr iterable;
        ExpressionPtr body;
        SourceSpan span;
    };

    struct Function {
        struct Parameter {
            std::string name;
            SourceSpan span;
        };

        std::vector<Parameter> parameters;
        ExpressionPtr body;
        SourceSpan span;
    };

    using Node = std::variant<Number, Boolean, String, Prefix, Infix, Grouping, Unit, Assign, Variable, Block, If, While, For, Function>;

    explicit Expression(Number node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Boolean node) : span(node.span), node(std::move(node)) {}
    explicit Expression(String node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Variable node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Prefix node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Infix node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Grouping node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Unit node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Assign node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Block node) : span(node.span), node(std::move(node)) {}
    explicit Expression(If node) : span(node.span), node(std::move(node)) {}
    explicit Expression(While node) : span(node.span), node(std::move(node)) {}
    explicit Expression(For node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Function node) : span(node.span), node(std::move(node)) {}

    ~Expression();

    SourceSpan span;
    Node node;
};

struct Statement {
    struct Let {
        std::string name;
        SourceSpan name_span;
        ExpressionPtr value;
        SourceSpan span;
    };

    struct Expression {
        ExpressionPtr expression;
        SourceSpan span;
    };

    using Node = std::variant<Let, Expression>;

    explicit Statement(Let node) : span(node.span), node(std::move(node)) {}
    explicit Statement(Expression node) : span(node.span), node(std::move(node)) {}

    ~Statement();

    SourceSpan span;
    Node node;
};

inline Expression::~Expression() = default;
inline Statement::~Statement() = default;

}  // namespace sonar
