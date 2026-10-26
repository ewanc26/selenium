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
    functionArities.clear();
    predeclaredGlobals.clear();
    predeclaredEnumerators.clear();
    // Predeclare global variables and functions so forward references work.
    for (const auto &item : prog.items) {
        if (!item) continue;
        if (const auto *global = std::get_if<VarDeclStmt>(&item->node)) {
            if (!scopes.front().insert(global->name).second) {
                error("redeclaration of variable '" + global->name + "'", item->line, item->col);
            }
            predeclaredGlobals.insert(global->name);
        } else if (const auto *enumeration = std::get_if<EnumDeclStmt>(&item->node)) {
            for (const auto &entry : enumeration->enumerators) {
                if (!scopes.front().insert(entry.first).second) {
                    error("redeclaration of global identifier '" + entry.first + "'", item->line, item->col);
                }
                predeclaredEnumerators.insert(entry.first);
            }
        } else if (const auto *fn = std::get_if<FunctionDeclStmt>(&item->node)) {
            functions.insert(fn->name);
            auto [it, inserted] = functionArities.emplace(fn->name, fn->params.size());
            if (!inserted && it->second != fn->params.size()) {
                error("conflicting parameter count for function '" + fn->name + "'", item->line, item->col);
            }
        }
    }
    loopDepth = 0;
    switchDepth = 0;
    inFunction = false;
    currentReturnType.reset();
    currentLabels.clear();
    for (const auto &item : prog.items) {
        if (item && !std::holds_alternative<FunctionDeclStmt>(item->node)) collectLabelsStmt(item);
    }
    for (const auto &item : prog.items) analyzeStmt(item);
}

void SemanticAnalyzer::analyzeBlock(std::shared_ptr<Block> block, bool createScope) {
    if (!block) return;
    if (createScope) scopes.emplace_back();
    for (const auto &stmt : block->stmts) analyzeStmt(stmt);
    if (createScope) scopes.pop_back();
}

void SemanticAnalyzer::collectLabelsBlock(std::shared_ptr<Block> block) {
    if (!block) return;
    for (const auto &stmt : block->stmts) collectLabelsStmt(stmt);
}

void SemanticAnalyzer::collectLabelsStmt(std::shared_ptr<Stmt> stmt) {
    if (!stmt) return;
    std::visit([&](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LabelStmt>) {
            if (!currentLabels.insert(node.label).second) {
                error("duplicate label '" + node.label + "'", stmt->line, stmt->col);
            }
            collectLabelsStmt(node.stmt);
        } else if constexpr (std::is_same_v<T, IfStmt>) {
            collectLabelsBlock(node.thenBlock);
            collectLabelsBlock(node.elseBlock);
        } else if constexpr (std::is_same_v<T, WhileStmt>) {
            collectLabelsBlock(node.body);
        } else if constexpr (std::is_same_v<T, DoWhileStmt>) {
            collectLabelsBlock(node.body);
        } else if constexpr (std::is_same_v<T, ForStmt>) {
            collectLabelsStmt(node.init);
            collectLabelsBlock(node.body);
        } else if constexpr (std::is_same_v<T, SwitchStmt>) {
            for (const auto &item : node.cases) collectLabelsBlock(item.body);
        } else if constexpr (std::is_same_v<T, FunctionDeclStmt>) {
            // Function labels are collected separately so a goto cannot cross function boundaries.
        }
    }, stmt->node);
}

void SemanticAnalyzer::analyzeStmt(std::shared_ptr<Stmt> stmt) {
    if (!stmt) return;
    std::visit([&](auto &node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, VarDeclStmt>) {
            const bool wasPredeclaredGlobal = scopes.size() == 1 && predeclaredGlobals.erase(node.name) != 0;
            if (!wasPredeclaredGlobal && scopes.back().find(node.name) != scopes.back().end()) {
                error("redeclaration of variable '" + node.name + "'", stmt->line, stmt->col);
            }
            if (!wasPredeclaredGlobal) scopes.back().insert(node.name);
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
            if (currentReturnType && currentReturnType->kind == TypeKind::Void && node.value) {
                error("void function cannot return a value", stmt->line, stmt->col);
            }
            if (currentReturnType && currentReturnType->kind != TypeKind::Void && !node.value) {
                error("non-void function must return a value", stmt->line, stmt->col);
            }
            analyzeExpr(node.value);
        } else if constexpr (std::is_same_v<T, BreakStmt>) {
            if (loopDepth == 0 && switchDepth == 0) {
                error("break outside a loop or switch", stmt->line, stmt->col);
            }
        } else if constexpr (std::is_same_v<T, ContinueStmt>) {
            if (loopDepth == 0) error("continue outside a loop", stmt->line, stmt->col);
        } else if constexpr (std::is_same_v<T, GotoStmt>) {
            if (currentLabels.find(node.label) == currentLabels.end()) {
                error("goto to undefined label '" + node.label + "'", stmt->line, stmt->col);
            }
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
                if (scopes.size() == 1 && predeclaredEnumerators.erase(name) != 0) continue;
                if (scopes.back().find(name) != scopes.back().end()) {
                    error("redeclaration of identifier '" + name + "'", stmt->line, stmt->col);
                }
                scopes.back().insert(name);
            }
        } else if constexpr (std::is_same_v<T, FunctionDeclStmt>) {
            if (!node.body) return;
            const bool previousFunction = inFunction;
            const auto previousReturnType = currentReturnType;
            const int previousLoopDepth = loopDepth;
            const int previousSwitchDepth = switchDepth;
            const auto previousLabels = currentLabels;
            currentLabels.clear();
            collectLabelsBlock(node.body);
            inFunction = true;
            currentReturnType = node.retType;
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
            currentReturnType = previousReturnType;
            currentLabels = previousLabels;
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
            auto arity = functionArities.find(node.name);
            if (arity == functionArities.end()) {
                error("call to undeclared function '" + node.name + "'", expr->line, expr->col);
            }
            if (arity->second != node.args.size()) {
                error("function '" + node.name + "' expects " + std::to_string(arity->second) +
                      " argument(s), got " + std::to_string(node.args.size()), expr->line, expr->col);
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
