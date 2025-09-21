#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sonar {

struct SourceSpan {
    std::size_t start{0};
    std::size_t end{0};
};

struct SourceLocation {
    std::size_t line{1};
    std::size_t column{1};
};

enum class TokenType {
    Number,
    String,
    Identifier,
    Let,
    Fn,
    If,
    Else,
    For,
    While,
    True,
    False,
    Plus,
    Minus,
    Star,
    Slash,
    Ampersand,
    Pipe,
    AndAnd,
    OrOr,
    LeftParen,
    RightParen,
    Comma,
    Colon,
    LeftBrace,
    RightBrace,
    Semicolon,
    Arrow,
    Equals,
    In,
    End,
};

struct Token {
    TokenType type{TokenType::End};
    std::string lexeme;
    SourceSpan span{};

    double as_number() const {
        try {
            size_t processed = 0;
            double value = std::stod(lexeme, &processed);
            if (processed != lexeme.size()) {
                throw std::invalid_argument("Trailing characters in number token");
            }
            return value;
        } catch (const std::exception& ex) {
            throw std::runtime_error(std::string("Failed to parse number '" + lexeme + "': ") + ex.what());
        }
    }
};

inline std::string to_string(TokenType type) {
    switch (type) {
        case TokenType::Number:
            return "number";
        case TokenType::String:
            return "string";
        case TokenType::Identifier:
            return "identifier";
        case TokenType::Let:
            return "let";
        case TokenType::Fn:
            return "fn";
        case TokenType::If:
            return "if";
        case TokenType::Else:
            return "else";
        case TokenType::For:
            return "for";
        case TokenType::While:
            return "while";
        case TokenType::True:
            return "true";
        case TokenType::False:
            return "false";
        case TokenType::Plus:
            return "+";
        case TokenType::Minus:
            return "-";
        case TokenType::Star:
            return "*";
        case TokenType::Slash:
            return "/";
        case TokenType::Ampersand:
            return "&";
        case TokenType::Pipe:
            return "|";
        case TokenType::AndAnd:
            return "&&";
        case TokenType::OrOr:
            return "||";
        case TokenType::LeftParen:
            return "(";
        case TokenType::RightParen:
            return ")";
        case TokenType::Comma:
            return ",";
        case TokenType::Colon:
            return ":";
        case TokenType::LeftBrace:
            return "{";
        case TokenType::RightBrace:
            return "}";
        case TokenType::Semicolon:
            return ";";
        case TokenType::Arrow:
            return "->";
        case TokenType::Equals:
            return "=";
        case TokenType::End:
            return "<eof>";
        case TokenType::In:
            return "in";
    }
    return "<unknown>";
}

}  // namespace sonar
