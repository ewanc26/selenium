#pragma once
#include <unordered_set>
#include "../ast/ast.h"
#include "../lexer/tokenizer.h"
#include <vector>

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens);
    Program parse();
private:
    const Token &peek(int offset = 0) const;
    const Token &advance();
    bool match(const std::string &k);
    bool check(const std::string &k) const;
    void expect(const std::string &k, const std::string &msg);
    std::shared_ptr<Stmt> parseTopLevel();
    std::shared_ptr<Stmt> parseStatement();
    std::shared_ptr<Stmt> parseVarDecl(bool mut);
    std::shared_ptr<Stmt> parseFunctionDecl(bool summon);
    std::shared_ptr<Stmt> parseTypeDecl();
    std::shared_ptr<Block> parseBlock();
    std::shared_ptr<Expr> parseExpression();
    std::shared_ptr<Expr> parseTernary();
    std::shared_ptr<Expr> parseLogicalOr();
    std::shared_ptr<Expr> parseLogicalAnd();
    std::shared_ptr<Expr> parseBitOr();
    std::shared_ptr<Expr> parseBitXor();
    std::shared_ptr<Expr> parseBitAnd();
    std::shared_ptr<Expr> parseEquality();
    std::shared_ptr<Expr> parseRelational();
    std::shared_ptr<Expr> parseShift();
    std::shared_ptr<Expr> parseAdditive();
    std::shared_ptr<Expr> parseMultiplicative();
    std::shared_ptr<Expr> parseCast();
    std::shared_ptr<Expr> parseUnary();
    std::shared_ptr<Expr> parsePostfix();
    std::shared_ptr<Expr> parsePrimary();
    std::shared_ptr<TypeRef> parseType();
    std::vector<Param> parseParams();
    const std::vector<Token> &tokens;
    size_t i = 0;
    std::unordered_set<std::string> typeNames;
};
#include <unordered_set>
