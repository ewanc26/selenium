#include "tokenizer.h"
#include <stdexcept>
#include <unordered_map>
#include <unordered_map>

static const std::unordered_map<std::string, std::string> KEYWORDS = {
    {"wax", "WAX"}, {"seal", "SEAL"}, {"ritual", "RITUAL"}, {"summon", "SUMMON"},
    {"from", "FROM"}, {"eclipse", "ECLIPSE"}, {"shadow", "SHADOW"},
    {"tide", "TIDE"}, {"undertow", "UNDERTOW"}, {"orbit", "ORBIT"},
    {"switch", "SWITCH"}, {"case", "CASE"}, {"default", "DEFAULT"},
    {"break", "BREAK"}, {"continue", "CONTINUE"}, {"drift", "DRIFT"},
    {"whisper", "WHISPER"}, {"return", "RETURN"}, {"cast", "CAST"},
    {"measure", "MEASURE"}, {"true", "TRUE"}, {"false", "FALSE"},
    {"nullptr", "NULLPTR"}, {"frame", "FRAME"}, {"fusion", "FUSION"},
    {"phase", "PHASE"}, {"alias", "ALIAS"},
    {"int", "TYPE"}, {"float", "TYPE"}, {"single", "TYPE"}, {"ldouble", "TYPE"},
    {"bool", "TYPE"}, {"schar", "TYPE"}, {"uchar", "TYPE"}, {"ushort", "TYPE"},
    {"uint", "TYPE"}, {"ulong", "TYPE"}, {"char", "TYPE"}, {"string", "TYPE"},
    {"void", "TYPE"}, {"long", "TYPE"}, {"short", "TYPE"}, {"double", "TYPE"},
    {"signed", "SIGNED"}, {"unsigned", "UNSIGNED"}, {"const", "CONST"},
    {"volatile", "VOLATILE"}, {"static", "STATIC"}, {"extern", "EXTERN"},
    {"inline", "INLINE"}, {"restrict", "RESTRICT"}, {"auto", "AUTO"},
    {"struct", "STRUCT"}, {"union", "UNION"}, {"enum", "ENUM"}, {"typedef", "TYPEDEF"},
    {"sizeof", "SIZEOF"}, {"goto", "GOTO"}, {"if", "IF"}, {"else", "ELSE"},
    {"while", "WHILE"}, {"do", "DO"}, {"for", "FOR"}, {"break", "BREAK"},
    {"continue", "CONTINUE"}, {"return", "RETURN"}, {"switch", "SWITCH"},
    {"case", "CASE"}, {"default", "DEFAULT"}, {"alignas", "ALIGNAS"},
    {"alignof", "ALIGNOF"}, {"_Alignas", "ALIGNAS"}, {"_Alignof", "ALIGNOF"},
    {"_Bool", "BOOL"}, {"_Static_assert", "STATIC_ASSERT"}, {"static_assert", "STATIC_ASSERT"},
    {"_Thread_local", "THREAD_LOCAL"}, {"thread_local", "THREAD_LOCAL"},
    {"_Noreturn", "NORETURN"}, {"_Atomic", "ATOMIC"}, {"_Generic", "GENERIC"},
    {"_Complex", "COMPLEX"}, {"_Imaginary", "IMAGINARY"}, {"asm", "ASM"},
};

Tokenizer::Tokenizer(const std::string &s) : source(s) {}

char Tokenizer::peek(int offset) const {
    if (i + offset >= source.size()) return 0;
    return source[i + offset];
}

char Tokenizer::advance() {
    if (i >= source.size()) return 0;
    char c = source[i++];
    if (c == '\n') { line++; col = 1; } else { col++; }
    return c;
}

bool Tokenizer::eof() const { return i >= source.size(); }

void Tokenizer::skipWhitespace() {
    while (!eof() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) advance();
}

void Tokenizer::skipLineComment() {
    advance(); advance(); // //
    while (!eof() && peek() != '\n') advance();
}

void Tokenizer::skipBlockComment() {
    advance(); advance(); // /*
    while (!eof()) {
        if (peek() == '*' && peek(1) == '/') { advance(); advance(); break; }
        advance();
    }
}

Token Tokenizer::readIdentifier() {
    int sl = line, sc = col;
    std::string s;
    while (!eof() && (std::isalnum(peek()) || peek() == '_')) s += advance();
    auto it = KEYWORDS.find(s);
    if (it != KEYWORDS.end()) {
        std::string k = it->second;
        if (k == "TRUE") return Token{"BOOL", "true", 1, 0, sl, sc};
        if (k == "FALSE") return Token{"BOOL", "false", 0, 0, sl, sc};
        return Token{k, s, 0, 0, sl, sc};
    }
    return Token{"IDENT", s, 0, 0, sl, sc};
}

Token Tokenizer::readNumber() {
    int sl = line, sc = col;
    std::string s;
    bool isFloat = false;
    if (peek() == '0' && peek(1) == 'x') {
        s += advance(); s += advance();
        while (!eof() && std::isxdigit(peek())) s += advance();
        // handle suffixes
        while (!eof() && std::isalpha(peek())) advance();
        long long v = std::stoll(s, nullptr, 16);
        return Token{"INT", s, v, 0, sl, sc};
    }
    if (peek() == '0' && peek(1) == 'b') {
        s += advance(); s += advance();
        while (!eof() && (peek() == '0' || peek() == '1')) s += advance();
        while (!eof() && std::isalpha(peek())) advance();
        long long v = 0;
        for (size_t j = 2; j < s.size(); j++) v = v*2 + (s[j]-'0');
        return Token{"INT", s, v, 0, sl, sc};
    }
    while (!eof() && std::isdigit(peek())) s += advance();
    if (peek() == '.') { isFloat = true; s += advance(); while (!eof() && std::isdigit(peek())) s += advance(); }
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true; s += advance();
        if (peek() == '+' || peek() == '-') s += advance();
        while (!eof() && std::isdigit(peek())) s += advance();
    }
    while (!eof() && std::isalpha(peek())) advance(); // suffixes
    if (isFloat) {
        return Token{"FLOAT", s, 0, std::stod(s), sl, sc};
    }
    // check for leading 0 octal
    if (s.size() > 1 && s[0] == '0') {
        long long v = 0;
        for (char c : s) if (c >= '0' && c <= '7') v = v*8 + (c-'0');
        return Token{"INT", s, v, 0, sl, sc};
    }
    long long v = std::stoll(s);
    return Token{"INT", s, v, 0, sl, sc};
}

Token Tokenizer::readString() {
    int sl = line, sc = col;
    advance(); // "
    std::string s;
    while (!eof() && peek() != '"') {
        if (peek() == '\\' && peek(1)) {
            advance();
            char e = advance();
            if (e == 'n') s += '\n';
            else if (e == 't') s += '\t';
            else if (e == 'r') s += '\r';
            else if (e == '\\') s += '\\';
            else if (e == '"') s += '"';
            else if (e == '0') s += '\0';
            else s += e;
        } else {
            s += advance();
        }
    }
    if (peek() == '"') advance();
    return Token{"STRING", s, 0, 0, sl, sc};
}

Token Tokenizer::readChar() {
    int sl = line, sc = col;
    advance(); // '
    char c = 0;
    if (peek() == '\\') {
        advance();
        char e = advance();
        if (e == 'n') c = '\n';
        else if (e == 't') c = '\t';
        else if (e == 'r') c = '\r';
        else if (e == '\\') c = '\\';
        else if (e == '\'') c = '\'';
        else if (e == '0') c = '\0';
        else c = e;
    } else {
        c = advance();
    }
    if (peek() == '\'') advance();
    return Token{"CHAR", std::string(1,c), c, 0, sl, sc};
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    while (!eof()) {
        skipWhitespace();
        if (eof()) break;
        if (peek() == '\n') { advance(); continue; }
        if (peek() == '/' && peek(1) == '/') { skipLineComment(); continue; }
        if (peek() == '/' && peek(1) == '*') { skipBlockComment(); continue; }
        int sl = line, sc = col;
        char c = peek();
        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentifier());
            continue;
        }
        if (std::isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }
        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }
        if (c == '\'') {
            tokens.push_back(readChar());
            continue;
        }
        // multi-char operators
        std::string two = std::string(1,c) + peek(1);
        std::string three = two + peek(2);
        if (three == "<<=" || three == ">>=" || three == "...") {
            advance(); advance(); advance();
            tokens.push_back(Token{three, three, 0,0,sl,sc});
            continue;
        }
        if (two == "->" || two == "<=" || two == ">=" || two == "==" || two == "!=" || 
            two == "&&" || two == "||" || two == "<<" || two == ">>" ||
            two == "+=" || two == "-=" || two == "*=" || two == "/=" || two == "%=" ||
            two == "&=" || two == "|=" || two == "^=" || two == "++" || two == "--") {
            advance(); advance();
            tokens.push_back(Token{two, two, 0,0,sl,sc});
            continue;
        }
        // single char
        advance();
        tokens.push_back(Token{std::string(1,c), std::string(1,c), 0,0,sl,sc});
    }
    tokens.push_back(Token{"EOF", "", 0,0,line,col});
    return tokens;
}
