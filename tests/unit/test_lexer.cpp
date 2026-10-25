#include <catch2/catch_test_macros.hpp>
#include "../../src/lexer/tokenizer.h"

TEST_CASE("lexer basic") {
    Tokenizer t("whisper \"hi\";");
    auto toks = t.tokenize();
    REQUIRE(toks.size() > 2);
}
