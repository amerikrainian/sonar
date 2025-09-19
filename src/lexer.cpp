#include "sonar/lexer.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sonar {

namespace {

bool is_digit(char ch) {
    return static_cast<bool>(std::isdigit(static_cast<unsigned char>(ch)));
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
            case '-':
                result.tokens.push_back({TokenType::Minus, "-", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '*':
                result.tokens.push_back({TokenType::Star, "*", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '/':
                result.tokens.push_back({TokenType::Slash, "/", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case '(':
                result.tokens.push_back({TokenType::LeftParen, "(", SourceSpan{index, index + 1}});
                ++index;
                continue;
            case ')':
                result.tokens.push_back({TokenType::RightParen, ")", SourceSpan{index, index + 1}});
                ++index;
                continue;
            default:
                break;
        }

        if (is_digit(ch) || ch == '.') {
            const std::size_t start = index;
            bool seen_dot = (ch == '.');
            ++index;
            while (index < source.size()) {
                const char next = source[index];
                if (is_digit(next)) {
                    ++index;
                    continue;
                }
                if (next == '.' && !seen_dot) {
                    seen_dot = true;
                    ++index;
                    continue;
                }
                break;
            }
            const std::size_t length = index - start;
            if (length == 1 && source[start] == '.') {
                throw make_error("Standalone '.' is not a valid number", start);
            }
            result.tokens.push_back({TokenType::Number, std::string(source.substr(start, length)), SourceSpan{start, start + length}});
            continue;
        }

        throw make_error("Unexpected character '" + std::string(1, ch) + "'", index);
    }

    result.tokens.push_back({TokenType::End, "", SourceSpan{source.size(), source.size()}});
    return result;
}

}  // namespace sonar
