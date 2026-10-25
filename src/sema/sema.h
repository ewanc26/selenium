#pragma once
#include "../ast/ast.h"
#include <string>
#include <unordered_set>
#include <vector>

class SemanticAnalyzer {
public:
    void analyze(Program &prog);
private:
    void analyzeStmt(std::shared_ptr<Stmt> stmt);
    void analyzeExpr(std::shared_ptr<Expr> expr);
    void analyzeBlock(std::shared_ptr<Block> block, bool createScope = true);
    bool declared(const std::string &name) const;
    [[noreturn]] void error(const std::string &message, int line, int col) const;

    std::vector<std::unordered_set<std::string>> scopes;
    int loopDepth = 0;
    int switchDepth = 0;
    bool inFunction = false;
};
