#include "sema.h"

#include <stdexcept>
#include <type_traits>
#include <utility>

bool SemanticAnalyzer::declared(const std::string &name) const {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if (scope->find(name) != scope->end()) return true;
    }
    return false;
}

[[noreturn]] void SemanticAnalyzer::error(const std::string &message, int line, int col) const {
    std::string location;
    if (line > 0) {
        location = " at line " + std::to_string(line);
        if (col > 0) location += ", column " + std::to_string(col);
    }
    throw std::runtime_error("semantic error: " + message + location);
}

void SemanticAnalyzer::analyze(Program &prog) {
    scopes.clear();
    scopes.emplace_back();
    functions.clear();
    // Collect all function declarations first so forward calls and recursion work.
    for (const auto &item : prog.items) {
        if (!item) continue;
        if (const auto *fn = std::get_if<FunctionDeclStmt>(&item->node)) {
            functions.insert(fn->name);
        }
    }
    loopDepth = 0;
    switchDepth = 0;
    inFunction = false;
    for (const auto &item : prog.items) analyzeStmt(item);
}

void SemanticAnalyzer::analyzeBlock(std::shared_ptr<Block> block, bool createScope) {
    if (!block) return;
    if (createScope) scopes.emplace_back();
    for (const auto &stmt : block->stmts) analyzeStmt(stmt);
    if (createScope) scopes.pop_back();
}

void SemanticAnalyzer::analyzeStmt(std::shared_ptr<Stmt> stmt) {
    if (!stmt) return;
    std::visit([&](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDeclStmt>) {
            if (scopes.back().find(node.name) != scopes.back().end()) {
                error("redeclaration of variable '" + node.name + "'", stmt->line, stmt->col);
            }
            scopes.back().insert(node.name);
            analyzeExpr(node.init);
        } else if constexpr (std::is_same_v<T, AssignStmt>) {
            analyzeExpr(node.target);
            analyzeExpr(node.value);
        } else if constexpr (std::is_same_v<T, CompoundAssignStmt>) {
            analyzeExpr(node.target);
            analyzeExpr(node.value);
        } else if constexpr (std::is_same_v<T, ExprStmt>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, PrintStmt>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, ReturnStmt>) {
            if (!inFunction) error("return outside a function", stmt->line, stmt->col);
            analyzeExpr(node.value);
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (loopDepth == 0 && switchDepth == 0) {
                error("break outside a loop or switch", stmt->line, stmt->col);
            }
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            if (loopDepth == 0) error("continue outside a loop", stmt->line, stmt->col);
        } else if constexpr (std::is_same_v<T, LabelStmt>) {
            analyzeStmt(node.stmt);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            analyzeExpr(node.cond);
            analyzeBlock(node.thenBlock);
            analyzeBlock(node.elseBlock);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            analyzeExpr(node.cond);
            ++loopDepth;
            analyzeBlock(node.body);
            --loopDepth;
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            ++loopDepth;
            analyzeBlock(node.body);
            --loopDepth;
            analyzeExpr(node.cond);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            scopes.emplace_back();
            analyzeStmt(node.init);
            analyzeExpr(node.cond);
            ++loopDepth;
            analyzeBlock(node.body);
            --loopDepth;
            analyzeExpr(node.incr);
            scopes.pop_back();
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            analyzeExpr(node.expr);
            ++switchDepth;
            for (const auto &item : node.cases) {
                analyzeExpr(item.value);
                analyzeBlock(item.body);
            }
            --switchDepth;
        } else if constexpr (std::is_same_v<T, EnumDeclStmt>) {
            for (const auto &enumerator : node.enumerators) {
                const auto &name = enumerator.first;
                if (scopes.back().find(name) != scopes.back().end()) {
                    error("redeclaration of identifier '" + name + "'", stmt->line, stmt->col);
                }
                scopes.back().insert(name);
            }
        } else if constexpr (std::is_same_v<T, FunctionDeclStmt>) {
            if (!node.body) return;
            const bool previousFunction = inFunction;
            const int previousLoopDepth = loopDepth;
            const int previousSwitchDepth = switchDepth;
            inFunction = true;
            loopDepth = 0;
            switchDepth = 0;
            scopes.emplace_back();
            for (const auto &param : node.params) {
                if (scopes.back().find(param.name) != scopes.back().end()) {
                    error("duplicate parameter '" + param.name + "'", stmt->line, stmt->col);
                }
                scopes.back().insert(param.name);
            }
            analyzeBlock(node.body, false);
            scopes.pop_back();
            inFunction = previousFunction;
            loopDepth = previousLoopDepth;
            switchDepth = previousSwitchDepth;
        } else {
            // Type declarations and labels do not introduce variable expressions.
        }
    }, stmt->node);
}

void SemanticAnalyzer::analyzeExpr(std::shared_ptr<Expr> expr) {
    if (!expr) return;
    std::visit([&](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarRef>) {
            if (!declared(node.name)) error("use of undeclared variable '" + node.name + "'", expr->line, expr->col);
        } else if constexpr (std::is_same_v<T, UnaryExpr>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, BinaryExpr>) {
            analyzeExpr(node.left);
            analyzeExpr(node.right);
        } else if constexpr (std::is_same_v<T, PostfixExpr>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, IndexExpr>) {
            analyzeExpr(node.base);
            analyzeExpr(node.index);
        } else if constexpr (std::is_same_v<T, MemberExpr>) {
            analyzeExpr(node.base);
        } else if constexpr (std::is_same_v<T, CallExpr>) {
            if (functions.find(node.name) == functions.end()) {
                error("call to undeclared function '" + node.name + "'", expr->line, expr->col);
            }
            for (const auto &arg : node.args) analyzeExpr(arg);
        } else if constexpr (std::is_same_v<T, CastExpr>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, SizeofExpr>) {
            analyzeExpr(node.expr);
        } else if constexpr (std::is_same_v<T, TernaryExpr>) {
            analyzeExpr(node.cond);
            analyzeExpr(node.thenExpr);
            analyzeExpr(node.elseExpr);
        } else {
            // Literal expressions require no name or control-flow validation.
        }
    }, expr->node);
}
