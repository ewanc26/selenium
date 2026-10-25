#include <catch2/catch_test_macros.hpp>
#include "../../src/lexer/tokenizer.h"
#include "../../src/parser/parser.h"

TEST_CASE("parser hello") {
    const std::string source = "whisper \"Hello, World!\";";
    Tokenizer t(source);
    auto toks = t.tokenize();
    REQUIRE(toks.size() == 4);
    REQUIRE(toks[0].kind == "WHISPER");
    REQUIRE(toks[1].kind == "STRING");
    REQUIRE(toks[2].kind == ";");
    Parser p(toks);
    auto prog = p.parse();
    REQUIRE(prog.items.size() == 1);
}
