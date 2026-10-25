#pragma once
#include "../ast/ast.h"

class SemanticAnalyzer {
public:
    void analyze(Program &prog);
private:
    void analyzeStmt(std::shared_ptr<Stmt> stmt);
    void analyzeExpr(std::shared_ptr<Expr> expr);
};
