#include "sonar/lexer.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace sonar {

namespace {

bool is_digit(char ch) {
    return static_cast<bool>(std::isdigit(static_cast<unsigned char>(ch)));
}

inline TokenType keyword_or_identifier(std::string_view lexeme) {
    static const std::unordered_map<std::string_view, TokenType> keywords = {
        {"let", TokenType::Let},
        {"fn", TokenType::Fn},
        {"if", TokenType::If},
        {"else", TokenType::Else},
        {"for", TokenType::For},
        {"while", TokenType::While},
        {"in", TokenType::In},
        {"true", TokenType::True},
        {"false", TokenType::False},
    };

    auto it = keywords.find(lexeme);
    return (it != keywords.end()) ? it->second : TokenType::Identifier;
}

Token scan_ident(std::string_view source, std::size_t& index) {
    const std::size_t start = index;
    ++index;
    while (index < source.size()) {
        char next = source[index];
        if (std::isalnum(static_cast<unsigned char>(next)) || next == '_') {
            ++index;
        } else {
            break;
        }
    }
    const std::size_t length = index - start;
    std::string_view lexeme = source.substr(start, length);
    TokenType type = keyword_or_identifier(lexeme);
    return {type, std::string(lexeme), SourceSpan{start, start + length}};
}

Token scan_number(std::string_view source, std::size_t& index, auto&& make_error) {
    const std::size_t start = index;
    bool seen_dot = false;

    // integer part or leading '.'
    if (source[index] == '.') {
        seen_dot = true;
        ++index;
        if (index >= source.size() || !is_digit(source[index])) {
            throw make_error("Standalone '.' is not a valid number", start);
        }
    } else {
        while (index < source.size() && is_digit(source[index])) {
            ++index;
        }
        if (index < source.size() && source[index] == '.') {
            seen_dot = true;
            ++index;
        }
    }

    // fractional part
    if (seen_dot) {
        while (index < source.size() && is_digit(source[index])) {
            ++index;
        }
    }

    // exponent part
    if (index < source.size() && (source[index] == 'e' || source[index] == 'E')) {
        ++index;
        if (index < source.size() && (source[index] == '+' || source[index] == '-')) {
            ++index;
        }
        if (index >= source.size() || !is_digit(source[index])) {
            throw make_error("Invalid exponent in number literal", index);
        }
        while (index < source.size() && is_digit(source[index])) {
            ++index;
        }
    }

    const std::size_t length = index - start;
    return {TokenType::Number, std::string(source.substr(start, length)), SourceSpan{start, start + length}};
}

template <typename ErrorMaker>
Token scan_string_literal(std::string_view source, std::size_t& index, ErrorMaker&& make_error,
                          [[maybe_unused]] std::vector<std::size_t>& line_offsets) {
    const std::size_t start = index;
    ++index;  // consume opening quote
    std::string value;

    while (index < source.size()) {
        char ch = source[index];
        if (ch == '"') {
            ++index;
            return {TokenType::String, std::move(value), SourceSpan{start, index}};
        }

        if (ch == '\\') {
            ++index;
            if (index >= source.size()) {
                throw make_error("Unterminated escape sequence in string literal", start);
            }
            char escape = source[index];
            ++index;
            switch (escape) {
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                case '"':
                    value.push_back('"');
                    break;
                default:
                    throw make_error("Unknown escape sequence '\\" + std::string(1, escape) + "'", index - 2);
            }
            continue;
        }

        if (ch == '\n') {
            throw make_error("Unterminated string literal", index);
        }

        value.push_back(ch);
        ++index;
    }

    throw make_error("Unterminated string literal", start);
}

template <typename ErrorMaker>
Token scan_raw_string_literal(std::string_view source, std::size_t& index, ErrorMaker&& make_error,
                              std::vector<std::size_t>& line_offsets) {
    const std::size_t start = index;
    ++index;  // consume 'r'

    std::size_t hash_count = 0;
    while (index < source.size() && source[index] == '#') {
        ++hash_count;
        ++index;
    }

    if (index >= source.size() || source[index] != '"') {
        throw make_error("Invalid raw string literal", index);
    }

    ++index;  // consume opening quote
    std::string value;

    while (index < source.size()) {
        char ch = source[index];
        if (ch == '"') {
            std::size_t closing_index = index + 1;
            std::size_t matched_hashes = 0;
            while (matched_hashes < hash_count && closing_index < source.size() && source[closing_index] == '#') {
                ++closing_index;
                ++matched_hashes;
            }
            if (matched_hashes == hash_count) {
                index = closing_index;
                return {TokenType::String, std::move(value), SourceSpan{start, index}};
            }
            value.push_back('"');
            ++index;
            continue;
        }

        if (ch == '\n') {
            ++index;
            line_offsets.push_back(index);
            value.push_back('\n');
            continue;
        }

        value.push_back(ch);
        ++index;
    }

    throw make_error("Unterminated raw string literal", start);
}

}  // namespace

LexResult Lexer::tokenize(std::string_view source) const {
    LexResult result;
    result.tokens.reserve(source.size());
    result.line_offsets.reserve(16);
    result.line_offsets.push_back(0);

    auto location_for = [&](std::size_t offset) {
        auto it = std::upper_bound(result.line_offsets.begin(), result.line_offsets.end(), offset);
        std::size_t line_index = (it == result.line_offsets.begin()) ? 0 : static_cast<std::size_t>(std::distance(result.line_offsets.begin(), it) - 1);
        std::size_t line = line_index + 1;
        std::size_t column = offset - result.line_offsets[line_index] + 1;
        return SourceLocation{line, column};
    };

    auto make_error = [&](const std::string& message, std::size_t offset) {
        auto location = location_for(offset);
        std::ostringstream oss;
        oss << message << " at line " << location.line << ", column " << location.column;
        return std::runtime_error(oss.str());
    };

    std::size_t index = 0;

    auto peek = [&](std::size_t lookahead = 0) -> char {
        return (index + lookahead < source.size()) ? source[index + lookahead] : '\0';
    };

    while (index < source.size()) {
        const char ch = source[index];
        if (ch == '\n') {
            ++index;
            result.line_offsets.push_back(index);
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch))) {
            ++index;
            continue;
        }

        switch (ch) {
            case '+':
                result.tokens.push_back({TokenType::Plus, "+", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '-': {
                if (peek(1) == '>') {
                    result.tokens.push_back({TokenType::Arrow, "->", SourceSpan{index, index + 2}});
                    index += 2;
                } else {
                    result.tokens.push_back({TokenType::Minus, "-", SourceSpan{index, index + 1}});
                    ++index;
                }
                continue;
            }
            case '*':
                result.tokens.push_back({TokenType::Star, "*", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '/': {
                if (peek(1) == '/') {
                    index += 2;
                    while (index < source.size() && peek() != '\n') {
                        ++index;
                    }
                    continue;
                } else if (peek(1) == '*') {
                    index += 2;
                    bool terminated = false;
                    while (index < source.size()) {
                        char current = peek();
                        if (current == '\n') {
                            ++index;
                            result.line_offsets.push_back(index);
                            continue;
                        }
                        if (current == '*' && peek(1) == '/') {
                            index += 2;  // consume */
                            terminated = true;
                            break;
                        }
                        ++index;
                    }
                    if (!terminated) {
                        throw make_error("Unterminated block comment", index);
                    }
                    continue;
                }

                result.tokens.push_back({TokenType::Slash, "/", SourceSpan{index, index + 1}});
                ++index;
                continue;
            }
            case '&': {
                if (peek(1) == '&') {
                    result.tokens.push_back({TokenType::AndAnd, "&&", SourceSpan{index, index + 2}});
                    index += 2;
                } else {
                    result.tokens.push_back({TokenType::Ampersand, "&", SourceSpan{index, index + 1}});
                    ++index;
                }
                continue;
            }
            case '|': {
                if (peek(1) == '|') {
                    result.tokens.push_back({TokenType::OrOr, "||", SourceSpan{index, index + 2}});
                    index += 2;
                } else {
                    result.tokens.push_back({TokenType::Pipe, "|", SourceSpan{index, index + 1}});
                    ++index;
                }
                continue;
            }
            case '(':
                result.tokens.push_back({TokenType::LeftParen, "(", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case ')':
                result.tokens.push_back({TokenType::RightParen, ")", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case ',':
                result.tokens.push_back({TokenType::Comma, ",", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '=':
                result.tokens.push_back({TokenType::Equals, "=", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case ':':
                result.tokens.push_back({TokenType::Colon, ":", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '{':
                result.tokens.push_back({TokenType::LeftBrace, "{", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '}':
                result.tokens.push_back({TokenType::RightBrace, "}", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case ';':
                result.tokens.push_back({TokenType::Semicolon, ";", SourceSpan{index, index + 1}});
                ++index;
                continue;
            default:
                break;
        }

        if (is_digit(ch) || ch == '.') {
            result.tokens.push_back(scan_number(source, index, make_error));
            continue;
        }

        if (ch == '"') {
            result.tokens.push_back(scan_string_literal(source, index, make_error, result.line_offsets));
            continue;
        }

        if (ch == 'r') {
            char next = peek(1);
            if (next == '"' || next == '#') {
                result.tokens.push_back(scan_raw_string_literal(source, index, make_error, result.line_offsets));
                continue;
            }
        }

        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
            result.tokens.push_back(scan_ident(source, index));
            continue;
        }

        throw make_error("Unexpected character '" + std::string(1, ch) + "'", index);
    }

    result.tokens.push_back({TokenType::End, "", SourceSpan{source.size(), source.size()}});
    return result;
}

}  // namespace sonar
