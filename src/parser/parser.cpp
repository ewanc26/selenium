#include "parser.h"
#include <stdexcept>
#include <unordered_set>

Parser::Parser(const std::vector<Token> &t) : tokens(t) {
    // known types
    typeNames = {"int","float","single","ldouble","bool","schar","uchar","char","string","void",
                 "long","short","unsigned","signed","double","uint","ushort","ulong"};
}

const Token &Parser::peek(int offset) const {
    if (i + offset < tokens.size()) return tokens[i + offset];
    return tokens.back();
}
const Token &Parser::advance() {
    if (i < tokens.size()) return tokens[i++];
    return tokens.back();
}
bool Parser::check(const std::string &k) const {
    return peek().kind == k;
}
bool Parser::match(const std::string &k) {
    if (check(k)) { advance(); return true; }
    return false;
}
void Parser::expect(const std::string &k, const std::string &msg) {
    if (!match(k)) throw std::runtime_error(msg + " at line " + std::to_string(peek().line));
}

Program Parser::parse() {
    Program prog;
    while (!check("EOF")) {
        prog.items.push_back(parseTopLevel());
    }
    return prog;
}

std::shared_ptr<Stmt> Parser::parseTopLevel() {
    if (check("RITUAL")) {
        advance();
        // check if summon style? no, RITUAL is definition
        return parseFunctionDecl(false);
    }
    if (check("SUMMON")) {
        advance();
        expect("RITUAL", "expected ritual after summon");
        return parseFunctionDecl(true);
    }
    if (check("WAX")) { advance(); return parseVarDecl(true); }
    if (check("SEAL")) { advance(); return parseVarDecl(false); }
    if (check("FRAME")) { advance(); return parseTypeDecl(); }
    if (check("FUSION")) { advance(); return parseTypeDecl(); }
    if (check("PHASE")) { advance(); return parseTypeDecl(); }
    if (check("ALIAS")) { advance(); return parseTypeDecl(); }
    return parseStatement();
}

std::shared_ptr<TypeRef> Parser::parseType() {
    auto t = std::make_shared<TypeRef>();
    // qualifiers
    while (check("CONST") || check("VOLATILE") || check("RESTRICT") || check("STATIC") || check("EXTERN")) {
        advance();
    }
    if (check("SIGNED")) { advance(); if (check("TYPE") && peek().value=="char") { advance(); t->kind = TypeKind::SChar; } else if (check("TYPE") && peek().value=="int") { advance(); t->kind = TypeKind::Int; } else { t->kind = TypeKind::Int; } }
    else if (check("UNSIGNED")) { 
        advance(); 
        if (check("TYPE")) {
            auto v = peek().value;
            if (v=="char") { advance(); t->kind = TypeKind::UChar; }
            else if (v=="short") { advance(); t->kind = TypeKind::UShort; }
            else if (v=="long") { advance(); t->kind = TypeKind::ULong; }
            else if (v=="int") { advance(); t->kind = TypeKind::UInt; }
            else { t->kind = TypeKind::UInt; }
        } else { t->kind = TypeKind::UInt; }
    }
    else if (check("TYPE")) {
        std::string v = peek().value;
        advance();
        if (v == "int") t->kind = TypeKind::Int;
        else if (v == "long") t->kind = TypeKind::Long;
        else if (v == "short") t->kind = TypeKind::Short;
        else if (v == "char") t->kind = TypeKind::Char;
        else if (v == "bool") t->kind = TypeKind::Bool;
        else if (v == "float") t->kind = TypeKind::Float;
        else if (v == "single") t->kind = TypeKind::Single;
        else if (v == "ldouble") t->kind = TypeKind::LDouble;
        else if (v == "string") t->kind = TypeKind::String;
        else if (v == "void") t->kind = TypeKind::Void;
        else if (v == "uint") t->kind = TypeKind::UInt;
        else if (v == "ushort") t->kind = TypeKind::UShort;
        else if (v == "ulong") t->kind = TypeKind::ULong;
        else if (v == "schar") t->kind = TypeKind::SChar;
        else if (v == "uchar") t->kind = TypeKind::UChar;
        else { t->kind = TypeKind::Named; t->name = v; }
    } else if (check("IDENT")) {
        std::string n = peek().value;
        // check if it's a known type name
        if (typeNames.count(n)) { advance(); t->kind = TypeKind::Named; t->name = n; }
        else { advance(); t->kind = TypeKind::Named; t->name = n; }
    } else {
        throw std::runtime_error("expected type at line " + std::to_string(peek().line));
    }
    // pointers
    while (match("*")) {
        auto inner = std::make_shared<TypeRef>();
        inner->kind = TypeKind::Ptr;
        inner->pointee = t;
        t = inner;
        // qualifiers after *
        while (check("CONST") || check("VOLATILE") || check("RESTRICT")) advance();
    }
    // arrays
    while (check("[")) {
        advance();
        if (check("INT")) { t->dims.push_back((int)peek().intValue); advance(); }
        else { t->dims.push_back(0); }
        expect("]", "expected ]");
    }
    return t;
}

std::shared_ptr<Stmt> Parser::parseVarDecl(bool mut) {
    auto t = parseType();
    std::string name = peek().value; expect("IDENT", "expected var name");
    std::shared_ptr<Expr> init;
    if (match("=")) {
        init = parseExpression();
    } else {
        // need initializer? selenium requires it in general? looking at examples - wax int x = 5;
        init = std::make_shared<Expr>(Expr{IntLit{0}});
    }
    expect(";", "expected ;");
    auto s = std::make_shared<Stmt>();
    s->node = VarDeclStmt{mut, t, name, init, {}};
    return s;
}

std::vector<Param> Parser::parseParams() {
    std::vector<Param> params;
    if (check(")")) return params;
    while (true) {
        auto t = parseType();
        std::string name = peek().value; expect("IDENT", "expected param name");
        params.push_back({t, name});
        if (!match(",")) break;
    }
    return params;
}

std::shared_ptr<Stmt> Parser::parseFunctionDecl(bool summon) {
    std::string name = peek().value; expect("IDENT", "expected function name");
    expect("(", "expected (");
    auto params = parseParams();
    expect(")", "expected )");
    expect("->", "expected ->");
    auto ret = parseType();
    std::optional<std::string> src;
    if (match("FROM")) {
        src = peek().value; expect("STRING", "expected string");
    }
    std::shared_ptr<Block> body;
    if (summon || src.has_value()) {
        expect(";", "expected ;");
        body = nullptr;
    } else {
        body = parseBlock();
        if (match(";")) {} // sometimes maybe
    }
    auto s = std::make_shared<Stmt>();
    s->node = FunctionDeclStmt{name, params, ret, body, src};
    return s;
}

std::shared_ptr<Stmt> Parser::parseTypeDecl() {
    // simplified
    std::string kind = peek(-1).kind; // rough
    // easier to handle by current token
    return parseStatement();
}

std::shared_ptr<Block> Parser::parseBlock() {
    expect("{", "expected {");
    auto b = std::make_shared<Block>();
    while (!check("}") && !check("EOF")) {
        b->stmts.push_back(parseStatement());
    }
    expect("}", "expected }");
    return b;
}

std::shared_ptr<Stmt> Parser::parseStatement() {
    if (check("WAX")) { advance(); return parseVarDecl(true); }
    if (check("SEAL")) { advance(); return parseVarDecl(false); }
    if (check("IF") || check("ECLIPSE")) {
        advance();
        expect("(", "expected (");
        auto cond = parseExpression();
        expect(")", "expected )");
        auto thenB = parseBlock();
        std::shared_ptr<Block> elseB;
        if (check("SHADOW") || check("ELSE")) { advance(); elseB = parseBlock(); }
        auto s = std::make_shared<Stmt>();
        s->node = IfStmt{cond, thenB, elseB};
        if (match(";")) {}
        return s;
    }
    if (check("WHILE") || check("TIDE")) { advance(); expect("(", "expected ("); auto cond = parseExpression(); expect(")", "expected )"); auto body = parseBlock(); auto s = std::make_shared<Stmt>(); s->node = WhileStmt{cond, body}; if (match(";")) {} return s; }
    if (check("UNDERTOW")) { advance(); auto body = parseBlock(); expect("TIDE", "expected tide"); expect("(", "expected ("); auto cond = parseExpression(); expect(")", "expected )"); auto s = std::make_shared<Stmt>(); s->node = DoWhileStmt{body, cond}; if (match(";")) {} return s; }
    if (check("FOR") || check("ORBIT")) { 
        advance(); expect("(", "expected ("); 
        std::shared_ptr<Stmt> init;
        if (check(";")) { init = nullptr; advance(); }
        else if (check("WAX")) { advance(); init = parseVarDecl(true); }
        else if (check("SEAL")) { advance(); init = parseVarDecl(false); }
        else { init = nullptr; /* maybe empty */ }
        auto cond = parseExpression(); expect(";", "expected ;");
        auto incr = parseExpression(); expect(")", "expected )");
        auto body = parseBlock();
        auto s = std::make_shared<Stmt>(); s->node = ForStmt{init, cond, incr, body};
        if (match(";")) {}
        return s;
    }
    if (check("SWITCH")) {
        advance(); expect("(", "expected ("); auto expr = parseExpression(); expect(")", "expected )");
        expect("{", "expected {");
        auto sw = std::make_shared<Stmt>();
        SwitchStmt ss; ss.expr = expr;
        while (!check("}") && !check("EOF")) {
            if (check("CASE")) {
                advance();
                auto val = parseExpression();
                expect(":", "expected :");
                auto b = std::make_shared<Block>();
                while (!check("CASE") && !check("DEFAULT") && !check("}")) {
                    b->stmts.push_back(parseStatement());
                }
                ss.cases.push_back(Case{val, b});
            } else if (check("DEFAULT")) {
                advance(); expect(":", "expected :");
                auto b = std::make_shared<Block>();
                while (!check("CASE") && !check("DEFAULT") && !check("}")) {
                    b->stmts.push_back(parseStatement());
                }
                // store as case with nullptr? we'll handle
                ss.cases.push_back(Case{nullptr, b});
            } else {
                break;
            }
        }
        expect("}", "expected }");
        if (match(";")) {}
        sw->node = ss;
        return sw;
    }
    if (check("BREAK")) { advance(); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = BreakStmt{}; return s; }
    if (check("CONTINUE")) { advance(); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = ContinueStmt{}; return s; }
    if (check("RETURN")) { advance(); std::shared_ptr<Expr> v; if (!check(";")) v = parseExpression(); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = ReturnStmt{v}; return s; }
    if (check("WHISPER")) { advance(); auto v = parseExpression(); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = PrintStmt{v}; return s; }
    if (check("GOTO")) { advance(); std::string lbl = peek().value; expect("IDENT", "expected label"); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = GotoStmt{lbl}; return s; }
    if (check("DRIFT")) { advance(); std::string lbl = peek().value; expect("IDENT", "expected label"); expect(";", "expected ;"); auto s = std::make_shared<Stmt>(); s->node = GotoStmt{lbl}; return s; } // drift is goto in selenium
    if (check("IDENT") && peek(1).kind == ":") {
        std::string lbl = peek().value; advance(); advance();
        auto inner = parseStatement();
        auto s = std::make_shared<Stmt>(); s->node = LabelStmt{lbl, inner}; return s;
    }
    // try expression statement or assignment
    auto expr = parseExpression();
    if (match("=")) {
        auto val = parseExpression();
        expect(";", "expected ;");
        auto s = std::make_shared<Stmt>(); s->node = AssignStmt{expr, val}; return s;
    }
    // compound assign
    if (check("+=")||check("-=")||check("*=")||check("/=")||check("%=")||check("&=")||check("|=")||check("^=")||check("<<=")||check(">>=")) {
        std::string op = peek().kind; advance();
        auto val = parseExpression();
        expect(";", "expected ;");
        auto s = std::make_shared<Stmt>(); s->node = CompoundAssignStmt{expr, op, val}; return s;
    }
    expect(";", "expected ;");
    auto s = std::make_shared<Stmt>(); s->node = ExprStmt{expr}; return s;
}

std::shared_ptr<Expr> Parser::parseExpression() { return parseTernary(); }
std::shared_ptr<Expr> Parser::parseTernary() {
    auto e = parseLogicalOr();
    if (match("?")) {
        auto t = parseExpression();
        expect(":", "expected :");
        auto f = parseExpression();
        auto res = std::make_shared<Expr>();
        res->node = TernaryExpr{e,t,f};
        return res;
    }
    return e;
}
std::shared_ptr<Expr> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match("||")) {
        auto right = parseLogicalAnd();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{BinaryOp::Or, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseLogicalAnd() {
    auto left = parseBitOr();
    while (match("&&")) {
        auto right = parseBitOr();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{BinaryOp::And, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseBitOr() {
    auto left = parseBitXor();
    while (match("|")) {
        auto right = parseBitXor();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{BinaryOp::BitOr, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseBitXor() {
    auto left = parseBitAnd();
    while (match("^")) {
        auto right = parseBitAnd();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{BinaryOp::BitXor, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseBitAnd() {
    auto left = parseEquality();
    while (match("&")) {
        auto right = parseEquality();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{BinaryOp::BitAnd, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseEquality() {
    auto left = parseRelational();
    while (check("==") || check("!=")) {
        std::string op = peek().kind; advance();
        auto right = parseRelational();
        auto res = std::make_shared<Expr>();
        res->node = BinaryExpr{op=="=="?BinaryOp::Eq:BinaryOp::Ne, left, right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseRelational() {
    auto left = parseShift();
    while (check("<")||check(">")||check("<=")||check(">=")) {
        std::string op = peek().kind; advance();
        auto right = parseShift();
        BinaryOp bop = BinaryOp::Lt;
        if (op=="<=") bop = BinaryOp::Le; else if (op==">") bop = BinaryOp::Gt; else if (op==">=") bop = BinaryOp::Ge;
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{bop,left,right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseShift() {
    auto left = parseAdditive();
    while (check("<<")||check(">>")) {
        std::string op = peek().kind; advance();
        auto right = parseAdditive();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{op=="<<"?BinaryOp::Shl:BinaryOp::Shr,left,right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (check("+")||check("-")) {
        std::string op = peek().kind; advance();
        auto right = parseMultiplicative();
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{op=="+"?BinaryOp::Add:BinaryOp::Sub,left,right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseMultiplicative() {
    auto left = parseCast();
    while (check("*")||check("/")||check("%")) {
        std::string op = peek().kind; advance();
        auto right = parseCast();
        BinaryOp bop = op=="*"?BinaryOp::Mul:(op=="/"?BinaryOp::Div:BinaryOp::Mod);
        auto res = std::make_shared<Expr>(); res->node = BinaryExpr{bop,left,right}; left = res;
    }
    return left;
}
std::shared_ptr<Expr> Parser::parseCast() {
    if (check("CAST")) {
        advance(); expect("(", "expected (");
        auto t = parseType();
        expect(",", "expected ,");
        auto e = parseExpression();
        expect(")", "expected )");
        auto res = std::make_shared<Expr>();
        // store as CastExpr with type
        auto ce = CastExpr{};
        // we'll convert type later
        res->node = ce;
        // easier to handle differently - just return e wrapped? for now
        return e;
    }
    if (check("MEASURE") || check("SIZEOF")) {
        advance();
        if (match("(")) {
            if (check("TYPE") || check("IDENT")) {
                auto t = parseType();
                expect(")", "expected )");
                auto res = std::make_shared<Expr>(); res->node = SizeofExpr{t, nullptr}; return res;
            }
            auto e = parseExpression(); expect(")", "expected )");
            auto res = std::make_shared<Expr>(); res->node = SizeofExpr{nullptr, e}; return res;
        }
        auto e = parseUnary();
        auto res = std::make_shared<Expr>(); res->node = SizeofExpr{nullptr, e}; return res;
    }
    return parseUnary();
}
std::shared_ptr<Expr> Parser::parseUnary() {
    if (check("++") || check("--")) {
        std::string op = peek().kind; advance();
        auto e = parseUnary();
        auto res = std::make_shared<Expr>();
        res->node = UnaryExpr{op=="++"?UnaryOp::PreInc:UnaryOp::PreDec, e};
        return res;
    }
    if (check("!") || check("~")) {
        std::string op = peek().kind; advance();
        auto e = parseCast();
        auto res = std::make_shared<Expr>();
        res->node = UnaryExpr{op=="!"?UnaryOp::Not:UnaryOp::BitNot, e};
        return res;
    }
    if (check("-")) { advance(); auto e = parseCast(); auto res = std::make_shared<Expr>(); res->node = UnaryExpr{UnaryOp::Neg, e}; return res; }
    if (check("+")) { advance(); return parseCast(); }
    if (check("&")) { advance(); auto e = parseCast(); auto res = std::make_shared<Expr>(); res->node = UnaryExpr{UnaryOp::AddrOf, e}; return res; }
    if (check("*")) { advance(); auto e = parseCast(); auto res = std::make_shared<Expr>(); res->node = UnaryExpr{UnaryOp::Deref, e}; return res; }
    return parsePostfix();
}
std::shared_ptr<Expr> Parser::parsePostfix() {
    auto e = parsePrimary();
    while (true) {
        if (match("[")) {
            auto idx = parseExpression();
            expect("]", "expected ]");
            auto res = std::make_shared<Expr>(); res->node = IndexExpr{e, idx}; e = res;
        } else if (match(".")) {
            std::string m = peek().value; expect("IDENT", "expected member"); auto res = std::make_shared<Expr>(); res->node = MemberExpr{e, m, false}; e = res;
        } else if (match("->")) {
            std::string m = peek().value; expect("IDENT", "expected member"); auto res = std::make_shared<Expr>(); res->node = MemberExpr{e, m, true}; e = res;
        } else if (check("++") || check("--")) {
            std::string op = peek().kind; advance();
            auto res = std::make_shared<Expr>(); res->node = PostfixExpr{e, op=="++"?PostfixOp::PostInc:PostfixOp::PostDec}; e = res;
        } else if (check("(") && std::holds_alternative<VarRef>(e->node)) {
            // function call
            auto &vr = std::get<VarRef>(e->node);
            advance(); // (
            std::vector<std::shared_ptr<Expr>> args;
            if (!check(")")) {
                while (true) {
                    args.push_back(parseExpression());
                    if (!match(",")) break;
                }
            }
            expect(")", "expected )");
            auto res = std::make_shared<Expr>(); res->node = CallExpr{vr.name, args}; e = res;
        } else {
            break;
        }
    }
    return e;
}
std::shared_ptr<Expr> Parser::parsePrimary() {
    if (check("INT")) {
        auto t = peek(); advance();
        auto e = std::make_shared<Expr>(); e->node = IntLit{t.intValue}; return e;
    }
    if (check("FLOAT")) {
        auto t = peek(); advance();
        auto e = std::make_shared<Expr>(); e->node = FloatLit{t.floatValue}; return e;
    }
    if (check("CHAR")) {
        auto t = peek(); advance();
        auto e = std::make_shared<Expr>(); e->node = CharLit{(char)t.intValue}; return e;
    }
    if (check("STRING")) {
        auto t = peek(); advance();
        auto e = std::make_shared<Expr>(); e->node = StringLit{t.value}; return e;
    }
    if (check("BOOL")) {
        auto t = peek(); advance();
        auto e = std::make_shared<Expr>(); e->node = BoolLit{t.intValue == 1}; return e;
    }
    if (check("NULLPTR")) { advance(); auto e = std::make_shared<Expr>(); e->node = NullptrLit{}; return e; }
    if (check("IDENT")) {
        std::string n = peek().value; advance();
        auto e = std::make_shared<Expr>(); e->node = VarRef{n}; return e;
    }
    if (check("(")) {
        advance();
        auto e = parseExpression();
        expect(")", "expected )");
        return e;
    }
    throw std::runtime_error("unexpected token " + peek().kind + " (\"" + peek().value + "\") in expression at line " + std::to_string(peek().line));
}
