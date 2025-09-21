#include <gtest/gtest.h>

#include <exception>
#include <string>
#include <utility>

#include "sonar/lexer.hpp"
#include "sonar/parser.hpp"
#include "sonar/pretty_printer.hpp"

namespace {

std::string parse_and_print(const std::string& source) {
    sonar::Lexer lexer;
    auto lex_result = lexer.tokenize(source);
    sonar::Parser parser(std::move(lex_result.tokens), std::move(lex_result.line_offsets), "<test>");
    auto ast = parser.parse();
    return sonar::pretty_print(*ast);
}

struct GoldenCase {
    const char* source;
    const char* expected;
};

void expect_golden(const GoldenCase& test_case) {
    SCOPED_TRACE(test_case.source);
    try {
        const auto actual = parse_and_print(test_case.source);
        EXPECT_EQ(actual, test_case.expected);
    } catch (const std::exception& ex) {
        ADD_FAILURE() << "Unexpected exception while parsing: " << ex.what();
    }
}

void expect_parse_error(const std::string& source, const std::string& message) {
    sonar::Lexer lexer;
    auto lex_result = lexer.tokenize(source);
    sonar::Parser parser(std::move(lex_result.tokens), std::move(lex_result.line_offsets), "<test>");
    try {
        auto ast = parser.parse();
        ADD_FAILURE() << "Expected parse error but parsed: " << sonar::pretty_print(*ast);
    } catch (const sonar::ParseError& err) {
        EXPECT_STREQ(message.c_str(), err.what());
    }
}

}  // namespace

TEST(PrettyPrinterGoldenTest, HandlesExpressionsAndOperators) {
    const GoldenCase cases[] = {
        {"true && false || true", "(|| (&& true false) true)"},
        {"1 | 2 & 3", "(| 1 (& 2 3))"},
        {"1 + 2 & 3", "(& (+ 1 2) 3)"},
        {"if true { 1 } else { 0 }", "(if true { 1 } else { 0 })"},
    };

    for (const auto& test_case : cases) {
        expect_golden(test_case);
    }
}

TEST(PrettyPrinterGoldenTest, HandlesStrings) {
    const GoldenCase cases[] = {
        {"\"hi\\nthere\"", "\"hi\\nthere\""},
        {"r\"\\n\"", "\"\\\\n\""},
        {R"(r#"a "quote" inside"#)", "\"a \\\"quote\\\" inside\""},
        {R"(r#"line1
line2"#)", "\"line1\\nline2\""},
    };

    for (const auto& test_case : cases) {
        expect_golden(test_case);
    }
}

TEST(PrettyPrinterGoldenTest, HandlesBlocksFunctionsAndLets) {
    const GoldenCase cases[] = {
        {"let flag = true && false || true;\nflag", "{ (let flag = (|| (&& true false) true)) flag }"},
        {"{ let message: string = \"hi\"; fn(name: string) -> string { message } }",
         "{ (let message: string = \"hi\") (fn (name: string) -> string { message }) }"},
    };

    for (const auto& test_case : cases) {
        expect_golden(test_case);
    }
}

TEST(ParserTypeAnnotationTest, RequiresParameterAnnotation) {
    expect_parse_error("fn(name) -> string name", "Expected ':' after parameter name");
}

TEST(ParserTypeAnnotationTest, RequiresReturnAnnotation) {
    expect_parse_error("fn(name: string) string", "Expected '->' after parameter list");
}

TEST(ParserTypeAnnotationTest, ReportsMissingTypeName) {
    expect_parse_error("let value: = 1;", "Expected type name");
}

TEST(ParserTypeAnnotationTest, ReportsMissingEqualsAfterAnnotation) {
    expect_parse_error("let value: number 1;", "Expected '=' after identifier (or type annotation)");
}
