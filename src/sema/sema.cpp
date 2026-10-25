#include "sema.h"
void SemanticAnalyzer::analyze(Program &prog) {
    for (auto &it : prog.items) analyzeStmt(it);
}
void SemanticAnalyzer::analyzeStmt(std::shared_ptr<Stmt> stmt) {
    if (!stmt) return;
    // basic pass-through for now
}
void SemanticAnalyzer::analyzeExpr(std::shared_ptr<Expr> expr) {
    if (!expr) return;
}
