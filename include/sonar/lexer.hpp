#pragma once

#include <string_view>
#include <vector>

#include "sonar/token.hpp"

namespace sonar {

struct LexResult {
    std::vector<Token> tokens;
    std::vector<std::size_t> line_offsets;
};

class Lexer {
   public:
    LexResult tokenize(std::string_view source) const;
};

}  // namespace sonar
