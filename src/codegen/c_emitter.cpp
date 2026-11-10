#include "c_emitter.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <unordered_map>

// Lightweight per-translation-unit variable type table. The emitter has no
// scope tree yet, so a flat name->kind map records the declared type of
// each variable as it is emitted; whisper uses it to pick the typed print
// entry point. Collisions across functions resolve last-declared-wins,
// which the current test corpus never exercises.
static std::unordered_map<std::string, TypeKind> &var_types() {
    static std::unordered_map<std::string, TypeKind> types;
    return types;
}

// Function return-type table, used by the same print inference.
static std::unordered_map<std::string, TypeKind> &func_ret_types() {
    static std::unordered_map<std::string, TypeKind> types;
    return types;
}

// The print family the runtime exposes; inferred from expression shape
// plus the variable table rather than a full type system.
enum class PType { Int, Float, Bool, Char, String };

static PType ptype_of_kind(TypeKind kind) {
    switch (kind) {
        case TypeKind::Float:
        case TypeKind::Single:
        case TypeKind::LDouble: return PType::Float;
        // Plain char prints its glyph; the sized char types print as
        // numbers, matching the C references.
        case TypeKind::Char: return PType::Char;
        case TypeKind::Bool: return PType::Bool;
        case TypeKind::String: return PType::String;
        default: return PType::Int;
    }
}

static PType infer_ptype(const std::shared_ptr<Expr> &e);

static PType infer_ptype_node(const ExprNode &node) {
    if (std::holds_alternative<IntLit>(node)) return PType::Int;
    if (std::holds_alternative<FloatLit>(node)) return PType::Float;
    if (std::holds_alternative<CharLit>(node)) return PType::Char;
    if (std::holds_alternative<StringLit>(node)) return PType::String;
    if (std::holds_alternative<BoolLit>(node)) return PType::Bool;
    if (std::holds_alternative<VarRef>(node)) {
        const auto &name = std::get<VarRef>(node).name;
        auto it = var_types().find(name);
        return it == var_types().end() ? PType::Int : ptype_of_kind(it->second);
    }
    if (std::holds_alternative<BinaryExpr>(node)) {
        const auto &b = std::get<BinaryExpr>(node);
        switch (b.op) {
            case BinaryOp::Lt:
            case BinaryOp::Le:
            case BinaryOp::Gt:
            case BinaryOp::Ge:
            case BinaryOp::Eq:
            case BinaryOp::Ne:
            case BinaryOp::And:
            case BinaryOp::Or:
                return PType::Bool;
            default:
                if (infer_ptype(b.left) == PType::Float || infer_ptype(b.right) == PType::Float) {
                    return PType::Float;
                }
                return PType::Int;
        }
    }
    if (std::holds_alternative<UnaryExpr>(node)) {
        const auto &u = std::get<UnaryExpr>(node);
        if (u.op == UnaryOp::Not) return PType::Bool;
        return infer_ptype(u.expr);
    }
    if (std::holds_alternative<PostfixExpr>(node)) {
        return infer_ptype(std::get<PostfixExpr>(node).expr);
    }
    if (std::holds_alternative<CallExpr>(node)) {
        const auto &name = std::get<CallExpr>(node).name;
        auto it = func_ret_types().find(name);
        return it == func_ret_types().end() ? PType::Int : ptype_of_kind(it->second);
    }
    if (std::holds_alternative<CastExpr>(node)) {
        const auto &c = std::get<CastExpr>(node);
        if (c.type) return ptype_of_kind(c.type->kind);
        return PType::Int;
    }
    if (std::holds_alternative<TernaryExpr>(node)) {
        return infer_ptype(std::get<TernaryExpr>(node).thenExpr);
    }
    if (std::holds_alternative<AssignExpr>(node)) {
        return infer_ptype(std::get<AssignExpr>(node).target);
    }
    return PType::Int;
}

static PType infer_ptype(const std::shared_ptr<Expr> &e) {
    if (!e) return PType::Int;
    return infer_ptype_node(e->node);
}

// Whether a top-level initializer may appear in a C global definition;
// anything computed is postponed into main's prologue instead.
static bool is_constant_init(const std::shared_ptr<Expr> &e) {
    if (!e) return true;
    if (std::holds_alternative<IntLit>(e->node) || std::holds_alternative<FloatLit>(e->node) ||
        std::holds_alternative<CharLit>(e->node) || std::holds_alternative<BoolLit>(e->node) ||
        std::holds_alternative<StringLit>(e->node) || std::holds_alternative<NullptrLit>(e->node)) {
        return true;
    }
    if (std::holds_alternative<UnaryExpr>(e->node)) {
        const auto &u = std::get<UnaryExpr>(e->node);
        return (u.op == UnaryOp::Neg || u.op == UnaryOp::BitNot) && is_constant_init(u.expr);
    }
    return false;
}

static std::string emitType(std::shared_ptr<TypeRef> t) {
    if (!t) return "void";
    if (t->kind == TypeKind::Ptr) {
        return emitType(t->pointee) + "*";
    }
    if (t->kind == TypeKind::Array) {
        return emitType(t->elem);
    }
    switch (t->kind) {
        case TypeKind::Int: return "int";
        case TypeKind::UInt: return "unsigned int";
        case TypeKind::Short: return "short";
        case TypeKind::UShort: return "unsigned short";
        case TypeKind::Long: return "long";
        case TypeKind::ULong: return "unsigned long";
        case TypeKind::SChar: return "signed char";
        case TypeKind::UChar: return "unsigned char";
        case TypeKind::Char: return "char";
        case TypeKind::Bool: return "_Bool";
        case TypeKind::Float: return "double";
        case TypeKind::Single: return "float";
        case TypeKind::LDouble: return "long double";
        case TypeKind::Void: return "void";
        case TypeKind::String: return "const char *";
        case TypeKind::Named: return t->name;
        default: return "int";
    }
}

static std::string emitExpr(std::shared_ptr<Expr> e);

static std::string floatLiteral(double value) {
    // Shortest decimal form that round-trips the exact double, always with
    // a '.' or exponent so the emitted token stays a floating literal.
    char buf[64];
    for (int prec = 1; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, value);
        if (std::strtod(buf, nullptr) == value) break;
    }
    std::string s = buf;
    if (s.find('.') == std::string::npos && s.find_first_of("eEpP") == std::string::npos) s += ".0";
    return s;
}

static std::string emitExprNode(const ExprNode &node) {
    if (std::holds_alternative<IntLit>(node)) return std::to_string(std::get<IntLit>(node).value);
    if (std::holds_alternative<FloatLit>(node)) return floatLiteral(std::get<FloatLit>(node).value);
    if (std::holds_alternative<CharLit>(node)) {
        char c = std::get<CharLit>(node).value;
        std::string s = "'";
        if (c == '\n') s += "\\n"; else if (c == '\t') s += "\\t"; else if (c == '\'') s += "\\'"; else s += c;
        s += "'";
        return s;
    }
    if (std::holds_alternative<StringLit>(node)) {
        std::string v = std::get<StringLit>(node).value;
        std::string out = "\"";
        for (char c : v) {
            if (c == '\n') { out += "\\n"; }
            else if (c == '\t') { out += "\\t"; }
            else if (c == '\r') { out += "\\r"; }
            else if (c == '"') { out += "\\\""; }
            else if (c == '\\') { out += "\\\\"; }
            else out += c;
        }
        out += "\"";
        return out;
    }
    if (std::holds_alternative<BoolLit>(node)) return std::get<BoolLit>(node).value ? "1" : "0";
    if (std::holds_alternative<NullptrLit>(node)) return "NULL";
    if (std::holds_alternative<VarRef>(node)) return std::get<VarRef>(node).name;
    if (std::holds_alternative<UnaryExpr>(node)) {
        auto &u = std::get<UnaryExpr>(node);
        std::string op;
        if (u.op == UnaryOp::Neg) op = "-";
        else if (u.op == UnaryOp::Not) op = "!";
        else if (u.op == UnaryOp::BitNot) op = "~";
        else if (u.op == UnaryOp::PreInc) op = "++";
        else if (u.op == UnaryOp::PreDec) op = "--";
        else if (u.op == UnaryOp::AddrOf) op = "&";
        else if (u.op == UnaryOp::Deref) op = "*";
        return op + emitExpr(u.expr);
    }
    if (std::holds_alternative<BinaryExpr>(node)) {
        auto &b = std::get<BinaryExpr>(node);
        std::string op;
        switch (b.op) {
            case BinaryOp::Add: op = "+"; break;
            case BinaryOp::Sub: op = "-"; break;
            case BinaryOp::Mul: op = "*"; break;
            case BinaryOp::Div: op = "/"; break;
            case BinaryOp::Mod: op = "%"; break;
            case BinaryOp::Lt: op = "<"; break;
            case BinaryOp::Le: op = "<="; break;
            case BinaryOp::Gt: op = ">"; break;
            case BinaryOp::Ge: op = ">="; break;
            case BinaryOp::Eq: op = "=="; break;
            case BinaryOp::Ne: op = "!="; break;
            case BinaryOp::And: op = "&&"; break;
            case BinaryOp::Or: op = "||"; break;
            case BinaryOp::BitAnd: op = "&"; break;
            case BinaryOp::BitOr: op = "|"; break;
            case BinaryOp::BitXor: op = "^"; break;
            case BinaryOp::Shl: op = "<<"; break;
            case BinaryOp::Shr: op = ">>"; break;
        }
        return "(" + emitExpr(b.left) + op + emitExpr(b.right) + ")";
    }
    if (std::holds_alternative<PostfixExpr>(node)) {
        auto &p = std::get<PostfixExpr>(node);
        return emitExpr(p.expr) + (p.op == PostfixOp::PostInc ? "++" : "--");
    }
    if (std::holds_alternative<IndexExpr>(node)) {
        auto &i = std::get<IndexExpr>(node);
        return emitExpr(i.base) + "[" + emitExpr(i.index) + "]";
    }
    if (std::holds_alternative<MemberExpr>(node)) {
        auto &m = std::get<MemberExpr>(node);
        return emitExpr(m.base) + (m.arrow ? "->" : ".") + m.member;
    }
    if (std::holds_alternative<CallExpr>(node)) {
        auto &c = std::get<CallExpr>(node);
        // Runtime input builtins map onto the runtime entry points.
        std::string callee = c.name;
        if (callee == "read_int") callee = "selenium_read_int";
        else if (callee == "read_float") callee = "selenium_read_float";
        else if (callee == "read_bool") callee = "selenium_read_bool";
        else if (callee == "read_char") callee = "selenium_read_char";
        std::string s = callee + "(";
        for (size_t i = 0; i < c.args.size(); i++) {
            if (i) s += ",";
            s += emitExpr(c.args[i]);
        }
        s += ")";
        return s;
    }
    if (std::holds_alternative<SizeofExpr>(node)) {
        auto &sz = std::get<SizeofExpr>(node);
        if (sz.type) return "sizeof(" + emitType(sz.type) + ")";
        if (sz.expr) return "sizeof(" + emitExpr(sz.expr) + ")";
        return "sizeof(int)";
    }
    if (std::holds_alternative<TernaryExpr>(node)) {
        auto &t = std::get<TernaryExpr>(node);
        return "(" + emitExpr(t.cond) + "?" + emitExpr(t.thenExpr) + ":" + emitExpr(t.elseExpr) + ")";
    }
    if (std::holds_alternative<AssignExpr>(node)) {
        auto &a = std::get<AssignExpr>(node);
        return "(" + emitExpr(a.target) + " = " + emitExpr(a.value) + ")";
    }
    if (std::holds_alternative<CastExpr>(node)) {
        auto &c = std::get<CastExpr>(node);
        return "((" + (c.type ? emitType(c.type) : c.typeName) + ")" + emitExpr(c.expr) + ")";
    }
    return "0";
}

static std::string emitExpr(std::shared_ptr<Expr> e) {
    if (!e) return "0";
    return emitExprNode(e->node);
}

static void emitStmt(std::shared_ptr<Stmt> stmt, std::stringstream &ss, int indent);

static std::string ind(int n) { return std::string(n*4, ' '); }

static void emitBlock(std::shared_ptr<Block> b, std::stringstream &ss, int indent) {
    if (!b) return;
    ss << " {\n";
    for (auto &st : b->stmts) emitStmt(st, ss, indent + 1);
    ss << ind(indent) << "}\n";
}

static void emitStmt(std::shared_ptr<Stmt> stmt, std::stringstream &ss, int indent) {
    if (!stmt) return;
    if (std::holds_alternative<VarDeclStmt>(stmt->node)) {
        auto &v = std::get<VarDeclStmt>(stmt->node);
        ss << ind(indent) << emitType(v.type) << " " << v.name;
        // handle arrays
        if (v.type->dims.size()) {
            for (auto d : v.type->dims) { if (d > 0) ss << "[" << d << "]"; else ss << "[]"; }
        } else if (v.type->kind == TypeKind::Array && v.type->elem) {
            // already handled
        }
        if (v.init) {
            // check if it's array init - simplified
            ss << " = " << emitExpr(v.init);
        }
        ss << ";\n";
        if (v.type) var_types()[v.name] = v.type->kind;
        return;
    }
    if (std::holds_alternative<PrintStmt>(stmt->node)) {
        auto &p = std::get<PrintStmt>(stmt->node);
        auto e = p.expr;
        std::string exprs = emitExpr(e);
        // Pick the runtime print entry point from the inferred type.
        switch (infer_ptype(e)) {
            case PType::Float:
                ss << ind(indent) << "selenium_print_float(" << exprs << ");\n";
                break;
            case PType::Bool:
                ss << ind(indent) << "selenium_print_bool(" << exprs << ");\n";
                break;
            case PType::Char:
                ss << ind(indent) << "selenium_print_char(" << exprs << ");\n";
                break;
            case PType::String:
                ss << ind(indent) << "selenium_print_string(" << exprs << ");\n";
                break;
            case PType::Int:
                ss << ind(indent) << "selenium_print_int(" << exprs << ");\n";
                break;
        }
        return;
    }
    if (std::holds_alternative<AssignStmt>(stmt->node)) {
        auto &a = std::get<AssignStmt>(stmt->node);
        ss << ind(indent) << emitExpr(a.target) << " = " << emitExpr(a.value) << ";\n";
        return;
    }
    if (std::holds_alternative<CompoundAssignStmt>(stmt->node)) {
        auto &a = std::get<CompoundAssignStmt>(stmt->node);
        ss << ind(indent) << emitExpr(a.target) << " " << a.op << " " << emitExpr(a.value) << ";\n";
        return;
    }
    if (std::holds_alternative<ExprStmt>(stmt->node)) {
        auto &e = std::get<ExprStmt>(stmt->node);
        ss << ind(indent) << emitExpr(e.expr) << ";\n";
        return;
    }
    if (std::holds_alternative<ReturnStmt>(stmt->node)) {
        auto &r = std::get<ReturnStmt>(stmt->node);
        if (r.value) ss << ind(indent) << "return " << emitExpr(r.value) << ";\n";
        else ss << ind(indent) << "return;\n";
        return;
    }
    if (std::holds_alternative<BreakStmt>(stmt->node)) { ss << ind(indent) << "break;\n"; return; }
    if (std::holds_alternative<ContinueStmt>(stmt->node)) { ss << ind(indent) << "continue;\n"; return; }
    if (std::holds_alternative<GotoStmt>(stmt->node)) { ss << ind(indent) << "goto " << std::get<GotoStmt>(stmt->node).label << ";\n"; return; }
    if (std::holds_alternative<LabelStmt>(stmt->node)) {
        auto &l = std::get<LabelStmt>(stmt->node);
        ss << ind(indent) << l.label << ":\n";
        if (l.stmt) emitStmt(l.stmt, ss, indent);
        return;
    }
    if (std::holds_alternative<IfStmt>(stmt->node)) {
        auto &i = std::get<IfStmt>(stmt->node);
        ss << ind(indent) << "if (" << emitExpr(i.cond) << ")";
        emitBlock(i.thenBlock, ss, indent);
        if (i.elseBlock) {
            ss << ind(indent) << "else";
            emitBlock(i.elseBlock, ss, indent);
        }
        return;
    }
    if (std::holds_alternative<WhileStmt>(stmt->node)) {
        auto &w = std::get<WhileStmt>(stmt->node);
        ss << ind(indent) << "while (" << emitExpr(w.cond) << ")";
        emitBlock(w.body, ss, indent);
        return;
    }
    if (std::holds_alternative<DoWhileStmt>(stmt->node)) {
        auto &d = std::get<DoWhileStmt>(stmt->node);
        ss << ind(indent) << "do";
        emitBlock(d.body, ss, indent);
        ss << ind(indent) << "while (" << emitExpr(d.cond) << ");\n";
        return;
    }
    if (std::holds_alternative<ForStmt>(stmt->node)) {
        auto &f = std::get<ForStmt>(stmt->node);
        ss << ind(indent) << "for (";
        if (f.init) {
            // emit init without semicolon
            if (std::holds_alternative<VarDeclStmt>(f.init->node)) {
                auto &v = std::get<VarDeclStmt>(f.init->node);
                ss << emitType(v.type) << " " << v.name;
                if (v.init) ss << " = " << emitExpr(v.init);
            } else {
                // expr
            }
        }
        ss << "; " << emitExpr(f.cond) << "; " << emitExpr(f.incr) << ")";
        emitBlock(f.body, ss, indent);
        return;
    }
    if (std::holds_alternative<SwitchStmt>(stmt->node)) {
        auto &sw = std::get<SwitchStmt>(stmt->node);
        ss << ind(indent) << "switch (" << emitExpr(sw.expr) << ") {\n";
        for (auto &c : sw.cases) {
            if (c.value) ss << ind(indent+1) << "case " << emitExpr(c.value) << ":\n";
            else ss << ind(indent+1) << "default:\n";
            if (c.body) {
                for (auto &st : c.body->stmts) emitStmt(st, ss, indent+2);
            }
        }
        ss << ind(indent) << "}\n";
        return;
    }

}

std::string emitProgram(const Program &prog) {
    std::stringstream ss;
    ss << "#include <stdio.h>\n#include <stdlib.h>\n#include <stdbool.h>\n#include <math.h>\n#include <string.h>\n\n";
    ss << "#ifndef NULL\n#define NULL ((void*)0)\n#endif\n\n";
    // include runtime
    ss << "void selenium_print_int(long long);\nvoid selenium_print_float(double);\nvoid selenium_print_bool(_Bool);\nvoid selenium_print_char(char);\nvoid selenium_print_string(const char *);\nint selenium_read_int();\ndouble selenium_read_float();\n_Bool selenium_read_bool();\nchar selenium_read_char();\n\n";
    
    // forward decls for functions
    std::vector<std::shared_ptr<Stmt>> funcs;
    std::vector<std::shared_ptr<Stmt>> others;
    for (auto &it : prog.items) {
        if (std::holds_alternative<FunctionDeclStmt>(it->node)) {
            funcs.push_back(it);
            auto &fn = std::get<FunctionDeclStmt>(it->node);
            if (fn.body) { // not summon
                ss << emitType(fn.retType) << " " << fn.name << "(";
                for (size_t i=0;i<fn.params.size();i++) {
                    if (i) ss << ", ";
                    ss << emitType(fn.params[i].type) << " " << fn.params[i].name;
                }
                ss << ");\n";
            } else {
                // summon
                ss << emitType(fn.retType) << " " << fn.name << "(";
                for (size_t i=0;i<fn.params.size();i++) {
                    if (i) ss << ", ";
                    ss << emitType(fn.params[i].type);
                    if (!fn.params[i].name.empty()) ss << " " << fn.params[i].name;
                }
                ss << ");\n";
                if (fn.source) {
                    // will be compiled separately
                }
            }
        } else {
            others.push_back(it);
        }
    }
    ss << "\n";

    // globals; non-constant initializers are postponed into main's
    // prologue, since C requires compile-time-constant global inits.
    std::vector<std::pair<std::string, std::shared_ptr<Expr>>> postponed_inits;
    for (auto &it : others) {
        if (std::holds_alternative<VarDeclStmt>(it->node)) {
            auto &v = std::get<VarDeclStmt>(it->node);
            ss << emitType(v.type) << " " << v.name;
            if (v.type->dims.size()) {
                for (auto d : v.type->dims) { if (d>0) ss << "[" << d << "]"; else ss << "[]"; }
            }
            const bool constant_init = !v.init || is_constant_init(v.init);
            if (v.init && constant_init) {
                ss << " = " << emitExpr(v.init);
            }
            ss << ";\n";
            if (v.init && !constant_init) {
                postponed_inits.emplace_back(v.name, v.init);
            }
            if (v.type) var_types()[v.name] = v.type->kind;
        } else if (std::holds_alternative<StructDeclStmt>(it->node)) {
            auto &s = std::get<StructDeclStmt>(it->node);
            ss << "struct " << s.name << " {\n";
            for (auto &f : s.fields) ss << "    " << emitType(f.type) << " " << f.name << ";\n";
            ss << "};\n";
        } else if (std::holds_alternative<UnionDeclStmt>(it->node)) {
            auto &s = std::get<UnionDeclStmt>(it->node);
            ss << "union " << s.name << " {\n";
            for (auto &f : s.fields) ss << "    " << emitType(f.type) << " " << f.name << ";\n";
            ss << "};\n";
        } else if (std::holds_alternative<EnumDeclStmt>(it->node)) {
            auto &e = std::get<EnumDeclStmt>(it->node);
            ss << "enum " << e.name << " {\n";
            for (size_t i=0;i<e.enumerators.size();i++) {
                if (i) ss << ",\n";
                ss << "    " << e.enumerators[i].first;
                if (e.enumerators[i].second.has_value()) ss << " = " << e.enumerators[i].second.value();
            }
            ss << "\n};\n";
        } else if (std::holds_alternative<AliasDeclStmt>(it->node)) {
            auto &a = std::get<AliasDeclStmt>(it->node);
            ss << "typedef " << emitType(a.target) << " " << a.name << ";\n";
        }
    }
    ss << "\n";
    
    // Libm builtins return doubles for typed printing.
    func_ret_types()["sin"] = TypeKind::Float;
    func_ret_types()["cos"] = TypeKind::Float;

    // function definitions
    for (auto &it : funcs) {
        auto &fn = std::get<FunctionDeclStmt>(it->node);
        if (fn.retType) func_ret_types()[fn.name] = fn.retType->kind;
        if (!fn.body) continue;
        ss << emitType(fn.retType) << " " << fn.name << "(";
        for (size_t i=0;i<fn.params.size();i++) {
            if (i) ss << ", ";
            ss << emitType(fn.params[i].type) << " " << fn.params[i].name;
        }
        ss << ")";
        // Register parameters so typed prints inside the body infer them.
        for (const auto &p : fn.params) {
            if (p.type) var_types()[p.name] = p.type->kind;
        }
        emitBlock(fn.body, ss, 0);
        ss << "\n";
    }
    
    // main
    ss << "int main() {\n";
    // Postponed top-level initializers run before any other statement.
    for (auto &[name, init] : postponed_inits) {
        ss << "    " << name << " = " << emitExpr(init) << ";\n";
    }
    for (auto &it : others) {
        if (std::holds_alternative<VarDeclStmt>(it->node)) {
            // Top-level variables were emitted as globals above.
            // for now skip duplicates? or emit
        } else if (std::holds_alternative<FunctionDeclStmt>(it->node)) continue;
        else if (std::holds_alternative<StructDeclStmt>(it->node) || std::holds_alternative<UnionDeclStmt>(it->node) ||
                 std::holds_alternative<EnumDeclStmt>(it->node) || std::holds_alternative<AliasDeclStmt>(it->node)) continue;
        else {
            // statement in main
            if (std::holds_alternative<VarDeclStmt>(it->node)) {
                // already emitted as global? better track
            }
        }
    }
    // easier: emit all non-decl items as statements in main? but better follow structure
    for (auto &it : others) {
        if (std::holds_alternative<VarDeclStmt>(it->node)) {
            // emit as local if it was put in main context? no easier to just re-emit logic
            continue;
        } else if (std::holds_alternative<StructDeclStmt>(it->node) || std::holds_alternative<UnionDeclStmt>(it->node) ||
                 std::holds_alternative<EnumDeclStmt>(it->node) || std::holds_alternative<AliasDeclStmt>(it->node) ||
                 std::holds_alternative<FunctionDeclStmt>(it->node)) continue;
        else {
            emitStmt(it, ss, 1);
        }
    }
    ss << "    return 0;\n}\n";
    return ss.str();
}
