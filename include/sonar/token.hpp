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
    Identifier,
    Let,
    Plus,
    Minus,
    Star,
    Slash,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Equals,
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
        case TokenType::Identifier:
            return "identifier";
        case TokenType::Let:
            return "let";
        case TokenType::Plus:
            return "+";
        case TokenType::Minus:
            return "-";
        case TokenType::Star:
            return "*";
        case TokenType::Slash:
            return "/";
        case TokenType::LeftParen:
            return "(";
        case TokenType::RightParen:
            return ")";
        case TokenType::End:
            return "<eof>";
        case TokenType::Equals:
            return "=";
        case TokenType::LeftBrace:
            return "{";
        case TokenType::RightBrace:
            return "}";
    }
    return "<unknown>";
}

}  // namespace sonar
