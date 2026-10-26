#pragma once
#include "../ast/ast.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SemanticAnalyzer {
public:
    void analyze(Program &prog);
private:
    void analyzeStmt(std::shared_ptr<Stmt> stmt);
    void analyzeExpr(std::shared_ptr<Expr> expr);
    void analyzeBlock(std::shared_ptr<Block> block, bool createScope = true);
    void collectLabelsStmt(std::shared_ptr<Stmt> stmt);
    void collectLabelsBlock(std::shared_ptr<Block> block);
    bool declared(const std::string &name) const;
    [[noreturn]] void error(const std::string &message, int line, int col) const;

    std::vector<std::unordered_set<std::string>> scopes;
    std::unordered_set<std::string> functions;
    std::unordered_set<std::string> functionDefinitions;
    std::unordered_map<std::string, std::size_t> functionArities;
    std::unordered_set<std::string> predeclaredGlobals;
    std::unordered_set<std::string> predeclaredEnumerators;
    std::unordered_set<std::string> currentLabels;
    int loopDepth = 0;
    int switchDepth = 0;
    bool inFunction = false;
    std::shared_ptr<TypeRef> currentReturnType;
};
