#include <catch2/catch_test_macros.hpp>
#include "../../src/lexer/tokenizer.h"
#include "../../src/parser/parser.h"
#include "../../src/sema/sema.h"

TEST_CASE("sema basic") {
    Tokenizer t("whisper 5;");
    auto toks = t.tokenize();
    Parser p(toks);
    auto prog = p.parse();
    SemanticAnalyzer sema;
    sema.analyze(prog);
    REQUIRE(true);
}
