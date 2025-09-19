#include <argparse/argparse.hpp>
#include <exception>
#include <fstream>
#include <iostream>
#include <replxx.hxx>
#include <sstream>
#include <string>

#include "sonar/lexer.hpp"
#include "sonar/parser.hpp"
#include "sonar/pretty_printer.hpp"

#ifndef SONAR_VERSION
#define SONAR_VERSION "0.0.0"
#endif

namespace {

void report_parse_error(const sonar::ParseError& error) {
    const auto location = error.location();
    std::cerr << error.source_name() << ":" << location.line << ":" << location.column
              << ": error: " << error.what() << std::endl;
}

void print_ast(const std::string& source, const std::string& source_name) {
    sonar::Lexer lexer;
    auto lex_result = lexer.tokenize(source);
    sonar::Parser parser(std::move(lex_result.tokens), std::move(lex_result.line_offsets), source_name);
    auto ast = parser.parse();
    std::cout << sonar::pretty_print(*ast) << std::endl;
}

void repl() {
    replxx::Replxx console;
    const std::string primary_prompt{"sonar> "};
    const std::string continuation_prompt{"... "};

    std::string prompt = primary_prompt;
    std::string buffer;
    std::size_t snippet_index = 1;
    std::string current_source_name;

    while (true) {
        const char* raw_input = console.input(prompt.c_str());
        if (raw_input == nullptr) {
            std::cout << "\n";
            break;
        }

        std::string line(raw_input);

        if (buffer.empty()) {
            current_source_name = "<repl #" + std::to_string(snippet_index) + ">";
            if (line == "quit" || line == "exit") {
                break;
            }
            if (line.find_first_not_of(" \t") == std::string::npos) {
                continue;
            }
        }

        if (!buffer.empty()) {
            buffer.push_back('\n');
        }
        buffer.append(line);

        try {
            print_ast(buffer, current_source_name);
            console.history_add(buffer);
            buffer.clear();
            prompt = primary_prompt;
            ++snippet_index;
        } catch (const sonar::ParseError& ex) {
            if (ex.incomplete()) {
                prompt = continuation_prompt;
                continue;
            }
            report_parse_error(ex);
            buffer.clear();
            prompt = primary_prompt;
            ++snippet_index;
        } catch (const std::exception& ex) {
            std::cerr << current_source_name << ": error: " << ex.what() << std::endl;
            buffer.clear();
            prompt = primary_prompt;
            ++snippet_index;
        }
    }
}

}  // namespace

int main(int argc, const char* const argv[]) {
    argparse::ArgumentParser program(
        argv[0], SONAR_VERSION,
        argparse::default_arguments::all, false);

    program.add_argument("file")
        .help("Parse this file instead of starting the REPL")
        .metavar("FILE")
        .nargs(argparse::nargs_pattern::optional);

    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (program["--version"] == true || program["--help"] == true) {
        return 0;
    }

    auto positional_file = program.present<std::string>("file");

    auto parse_file = [&](const std::string& path) -> int {
        std::ifstream input(path);
        if (!input) {
            std::cerr << "error: failed to open '" << path << "'" << std::endl;
            return 1;
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        std::string source = contents.str();
        try {
            print_ast(source, path);
        } catch (const sonar::ParseError& ex) {
            report_parse_error(ex);
            return 1;
        } catch (const std::exception& ex) {
            std::cerr << path << ": error: " << ex.what() << std::endl;
            return 1;
        }
        return 0;
    };

    if (positional_file) {
        return parse_file(*positional_file);
    }

    std::cout << "sonar " << SONAR_VERSION
              << " — enter an expression, or type 'quit' to exit."
              << std::endl;
    repl();
    return 0;
}
