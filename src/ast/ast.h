#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct Expr;
struct Stmt;

enum class TypeKind {
    Int,
    UInt,
    Short,
    UShort,
    Long,
    ULong,
    SChar,
    UChar,
    Char,
    Bool,
    Float,
    Single,
    LDouble,
    Void,
    String,
    Named,
    Ptr,
    Array,
    Func
};

struct TypeRef {
    TypeKind kind = TypeKind::Int;
    std::string name; // for Named types
    std::shared_ptr<TypeRef> pointee;
    std::shared_ptr<TypeRef> elem;
    std::vector<int> dims;
    int pointerCount = 0;
    // for function types
    std::vector<std::shared_ptr<TypeRef>> params;
    std::shared_ptr<TypeRef> ret;
};

struct Param {
    std::shared_ptr<TypeRef> type;
    std::string name;
};

struct IntLit { long long value; };
struct FloatLit { double value; };
struct CharLit { char value; };
struct StringLit { std::string value; };
struct BoolLit { bool value; };
struct NullptrLit {};

struct VarRef { std::string name; };

enum class UnaryOp { Neg, Not, BitNot, PreInc, PreDec, AddrOf, Deref };
struct UnaryExpr { UnaryOp op; std::shared_ptr<Expr> expr; };

enum class BinaryOp { Add, Sub, Mul, Div, Mod, Lt, Le, Gt, Ge, Eq, Ne, And, Or, BitAnd, BitOr, BitXor, Shl, Shr };
struct BinaryExpr { BinaryOp op; std::shared_ptr<Expr> left; std::shared_ptr<Expr> right; };

enum class PostfixOp { PostInc, PostDec };
struct PostfixExpr { std::shared_ptr<Expr> expr; PostfixOp op; };

struct IndexExpr { std::shared_ptr<Expr> base; std::shared_ptr<Expr> index; };
struct MemberExpr { std::shared_ptr<Expr> base; std::string member; bool arrow = false; };
struct CallExpr { std::string name; std::vector<std::shared_ptr<Expr>> args; };
struct CastExpr { std::string typeName; std::shared_ptr<Expr> expr; std::shared_ptr<TypeRef> type; };
struct SizeofExpr { std::shared_ptr<TypeRef> type; std::shared_ptr<Expr> expr; };
struct TernaryExpr { std::shared_ptr<Expr> cond; std::shared_ptr<Expr> thenExpr; std::shared_ptr<Expr> elseExpr; };

using ExprNode = std::variant<
    IntLit, FloatLit, CharLit, StringLit, BoolLit, NullptrLit,
    VarRef, UnaryExpr, BinaryExpr, PostfixExpr, IndexExpr, MemberExpr, CallExpr,
    CastExpr, SizeofExpr, TernaryExpr
>;

struct Expr {
    ExprNode node;
    int line = 0;
    int col = 0;
};

struct Block {
    std::vector<std::shared_ptr<Stmt>> stmts;
};

struct VarDeclStmt {
    bool mutableDecl = true;
    std::shared_ptr<TypeRef> type;
    std::string name;
    std::shared_ptr<Expr> init;
    std::vector<std::string> qualifiers;
};

struct AssignStmt { std::shared_ptr<Expr> target; std::shared_ptr<Expr> value; };
struct CompoundAssignStmt { std::shared_ptr<Expr> target; std::string op; std::shared_ptr<Expr> value; };

struct ExprStmt { std::shared_ptr<Expr> expr; };
struct PrintStmt { std::shared_ptr<Expr> expr; };
struct ReturnStmt { std::shared_ptr<Expr> value; };
struct BreakStmt {};
struct ContinueStmt {};
struct GotoStmt { std::string label; };
struct LabelStmt { std::string label; std::shared_ptr<Stmt> stmt; };

struct IfStmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Block> thenBlock;
    std::shared_ptr<Block> elseBlock;
};

struct WhileStmt {
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Block> body;
};

struct DoWhileStmt {
    std::shared_ptr<Block> body;
    std::shared_ptr<Expr> cond;
};

struct ForStmt {
    std::shared_ptr<Stmt> init; // can be var decl or expr
    std::shared_ptr<Expr> cond;
    std::shared_ptr<Expr> incr;
    std::shared_ptr<Block> body;
};

struct Case {
    std::shared_ptr<Expr> value; // nullptr for default
    std::shared_ptr<Block> body;
};

struct SwitchStmt {
    std::shared_ptr<Expr> expr;
    std::vector<Case> cases;
};

struct StructDeclStmt {
    std::string name;
    std::vector<Param> fields;
};

struct UnionDeclStmt {
    std::string name;
    std::vector<Param> fields;
};

struct EnumDeclStmt {
    std::string name;
    std::vector<std::pair<std::string, std::optional<long long>>> enumerators;
};

struct AliasDeclStmt {
    std::string name;
    std::shared_ptr<TypeRef> target;
};

struct FunctionDeclStmt {
    std::string name;
    std::vector<Param> params;
    std::shared_ptr<TypeRef> retType;
    std::shared_ptr<Block> body; // nullptr for extern
    std::optional<std::string> source; // from "file.c"
};

using StmtNode = std::variant<
    VarDeclStmt, AssignStmt, CompoundAssignStmt, ExprStmt, PrintStmt, ReturnStmt,
    BreakStmt, ContinueStmt, GotoStmt, LabelStmt, IfStmt, WhileStmt, DoWhileStmt,
    ForStmt, SwitchStmt, StructDeclStmt, UnionDeclStmt, EnumDeclStmt, AliasDeclStmt, FunctionDeclStmt
>;

struct Stmt {
    StmtNode node;
    int line = 0;
    int col = 0;
};

struct Program {
    std::vector<std::shared_ptr<Stmt>> items;
};
