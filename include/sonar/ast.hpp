#pragma once

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "sonar/token.hpp"

namespace sonar {

struct Expression {
    struct Number {
        double value;
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

    struct Let {
        std::string name;
        SourceSpan name_span;
        std::unique_ptr<Expression> value;
        SourceSpan span;
    };

    struct Assign {
        std::string name;
        SourceSpan name_span;
        std::unique_ptr<Expression> value;
        SourceSpan span;
    };

    struct Block {
        std::vector<std::unique_ptr<Expression>> expressions;
        SourceSpan span;
    };

    struct If {
        std::unique_ptr<Expression> condition;
        std::unique_ptr<Expression> then;
        std::unique_ptr<Expression> else_branch;
        SourceSpan span;
    };

    using Node = std::variant<Number, Prefix, Infix, Grouping, Unit, Let, Assign, Variable, Block, If>;

    explicit Expression(Number node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Variable node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Prefix node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Infix node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Grouping node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Unit node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Let node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Assign node) : span(node.span), node(std::move(node)) {}
    explicit Expression(Block node) : span(node.span), node(std::move(node)) {}
    explicit Expression(If node) : span(node.span), node(std::move(node)) {}

    SourceSpan span;
    Node node;
};

using ExpressionPtr = std::unique_ptr<Expression>;

}  // namespace sonar
