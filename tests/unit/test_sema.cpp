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

TEST_CASE("sema accepts a global variable referenced before declaration") {
    auto program = parse_source("whisper global; wax int global = 5;");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema accepts a function that references a later global variable") {
    auto program = parse_source("ritual read_global() -> int { return global; } wax int global = 5;");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects duplicate global variables") {
    auto program = parse_source("wax int global = 1; wax int global = 2;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("redeclaration of variable 'global'"));
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

TEST_CASE("sema rejects continue outside a loop") {
    auto program = parse_source("continue;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("continue outside a loop"));
}

TEST_CASE("sema rejects a value returned from a void function") {
    auto program = parse_source("ritual no_value() -> void { return 1; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("void function cannot return a value"));
}

TEST_CASE("sema rejects a missing value returned from a non-void function") {
    auto program = parse_source("ritual needs_value() -> int { return; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("non-void function must return a value"));
}

TEST_CASE("sema accepts an empty return in a void function") {
    auto program = parse_source("ritual no_value() -> void { return; }");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects return outside a function") {
    auto program = parse_source("return 1;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("return outside a function"));
}

TEST_CASE("sema rejects duplicate function parameters") {
    auto program = parse_source("ritual sample(int value, int value) -> int { return value; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("duplicate parameter 'value'"));
}

TEST_CASE("sema rejects duplicate function definitions") {
    auto program = parse_source("ritual sample() -> int { return 1; } ritual sample() -> int { return 2; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("duplicate definition of function 'sample'"));
}

TEST_CASE("sema rejects calls to undeclared functions") {
    auto program = parse_source("missing(1);");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("call to undeclared function 'missing'"));
}

TEST_CASE("sema accepts calls to functions declared later") {
    auto program = parse_source("sample(1); ritual sample(int value) -> int { return value; }");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects function calls with the wrong argument count") {
    auto program = parse_source("ritual sample(int value) -> int { return value; } sample();");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("expects 1 argument(s), got 0"));
}

TEST_CASE("sema accepts recursive function calls") {
    auto program = parse_source("ritual count(int value) -> int { eclipse (value) { return count(value - 1); } return 0; }");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema accepts a goto to a label in the same function") {
    auto program = parse_source("ritual sample(int value) -> int { drift finish; finish: return value; }");
    SemanticAnalyzer sema;
    REQUIRE_NOTHROW(sema.analyze(program));
}

TEST_CASE("sema rejects a goto to an undefined label") {
    auto program = parse_source("drift missing;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("goto to undefined label 'missing'"));
}

TEST_CASE("sema rejects duplicate labels in a function") {
    auto program = parse_source("ritual sample() -> void { finish: return; finish: return; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("duplicate label 'finish'"));
}

TEST_CASE("sema does not allow a goto to cross function boundaries") {
    auto program = parse_source("ritual first() -> void { shared: return; } ritual second() -> void { drift shared; return; }");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("goto to undefined label 'shared'"));
}

TEST_CASE("sema rejects a local variable after its scope ends") {
    auto program = parse_source("eclipse (true) { wax int local = 1; } whisper local;");
    SemanticAnalyzer sema;
    REQUIRE_THROWS_WITH(sema.analyze(program), Catch::Matchers::ContainsSubstring("undeclared variable 'local'"));
}
