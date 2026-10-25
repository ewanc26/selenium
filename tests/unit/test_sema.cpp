#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include "../../src/lexer/tokenizer.h"
#include "../../src/parser/parser.h"
#include "../../src/sema/sema.h"

namespace {
Program parse_source(const std::string &source) {
    Tokenizer tokenizer(source);
    auto tokens = tokenizer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}
}

TEST_CASE("sema accepts a declared variable") {
    auto program = parse_source("wax int value = 5; whisper value;");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects an undeclared variable") {
    auto program = parse_source("whisper missing;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("undeclared variable 'missing'"));
}

TEST_CASE("sema rejects duplicate variables in one scope") {
    auto program = parse_source("wax int value = 1; wax int value = 2;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("redeclaration of variable 'value'"));
}

TEST_CASE("sema enforces loop-control context") {
    auto program = parse_source("break;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("break outside a loop or switch"));
}

TEST_CASE("sema permits break inside a loop") {
    auto program = parse_source("tide (1) { break; }");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects a local variable after its scope ends") {
    auto program = parse_source("eclipse (true) { wax int local = 1; } whisper local;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("undeclared variable 'local'"));
}
