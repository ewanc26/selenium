#pragma once

#include <string>
#include <vector>

struct Token {
    std::string kind;
    std::string value;
    long long intValue = 0;
    double floatValue = 0;
    int line = 0;
    int col = 0;
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string &source);
    std::vector<Token> tokenize();
private:
    char peek(int offset = 0) const;
    char advance();
    bool eof() const;
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    Token readIdentifier();
    Token readNumber();
    Token readString();
    Token readChar();
    const std::string &source;
    size_t i = 0;
    int line = 1;
    int col = 1;
};
