from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional

from .ast import (
    Assign,
    Binary,
    Block,
    Call,
    Cast,
    Expr,
    ExprStmt,
    ForStmt,
    FunctionDecl,
    IfStmt,
    Literal,
    Param,
    PrintStmt,
    Program,
    ReturnStmt,
    Stmt,
    TopLevel,
    TypeRef,
    Unary,
    VarDecl,
    VarRef,
    WhileStmt,
)


class SemanticError(Exception):
    pass


@dataclass(slots=True)
class TypeInfo:
    name: str

    @property
    def is_numeric(self) -> bool:
        return self.name in {"int", "float"}

    @property
    def is_primitive(self) -> bool:
        return self.name in {"int", "float", "bool", "char", "string", "void"}


@dataclass(slots=True)
class Symbol:
    type: TypeInfo
    mutable: bool
    is_function: bool = False
    params: Optional[List[TypeInfo]] = None
    return_type: Optional[TypeInfo] = None


BUILTINS = {name: TypeInfo(name) for name in ("int", "float", "bool", "char", "string", "void")}


class Scope:
    def __init__(self, parent: Optional["Scope"] = None):
        self.parent = parent
        self.symbols: Dict[str, Symbol] = {}

    def define(self, name: str, symbol: Symbol) -> None:
        if name in self.symbols:
            raise SemanticError(f"Duplicate name: {name}")
        self.symbols[name] = symbol

    def lookup(self, name: str) -> Symbol:
        scope: Optional[Scope] = self
        while scope is not None:
            if name in scope.symbols:
                return scope.symbols[name]
            scope = scope.parent
        raise SemanticError(f"Undefined name: {name}")


@dataclass(slots=True)
class FunctionInfo:
    decl: FunctionDecl
    param_types: List[TypeInfo]
    return_type: TypeInfo


class SemanticAnalyzer:
    def __init__(self):
        self.globals = Scope()
        self.functions: Dict[str, FunctionInfo] = {}
        self.current_return: Optional[TypeInfo] = None
        self.expr_types: Dict[int, TypeInfo] = {}

    def analyze(self, program: Program) -> Program:
        for item in program.items:
            if isinstance(item, FunctionDecl):
                self._register_function(item)
        for item in program.items:
            self._analyze_top_level(item, self.globals)
        return program

    def _register_function(self, decl: FunctionDecl) -> None:
        if decl.name in self.functions or decl.name in self.globals.symbols:
            raise SemanticError(f"Duplicate function name: {decl.name}")
        param_types = [self._type_of_ref(p.type) for p in decl.params]
        return_type = self._type_of_ref(decl.return_type)
        self.functions[decl.name] = FunctionInfo(decl, param_types, return_type)
        self.globals.define(
            decl.name,
            Symbol(
                type=return_type,
                mutable=False,
                is_function=True,
                params=param_types,
                return_type=return_type,
            ),
        )

    def _analyze_top_level(self, item: TopLevel, scope: Scope) -> None:
        if isinstance(item, FunctionDecl):
            self._analyze_function(item)
            return
        self._analyze_stmt(item, scope, in_function=False)

    def _analyze_function(self, decl: FunctionDecl) -> None:
        info = self.functions[decl.name]
        fn_scope = Scope(self.globals)
        for param, ptype in zip(decl.params, info.param_types):
            fn_scope.define(param.name, Symbol(ptype, mutable=True))
        prev = self.current_return
        self.current_return = info.return_type
        self._analyze_block(decl.body, fn_scope, in_function=True)
        self.current_return = prev

    def _analyze_block(self, block: Block, scope: Scope, in_function: bool) -> None:
        child = Scope(scope)
        for stmt in block.statements:
            self._analyze_stmt(stmt, child, in_function)

    def _analyze_stmt(self, stmt: Stmt, scope: Scope, in_function: bool) -> None:
        if isinstance(stmt, VarDecl):
            value_type = self._infer_expr(stmt.value, scope)
            decl_type = self._type_of_ref(stmt.type)
            self._require_same_type(decl_type, value_type, f"Type mismatch in declaration of {stmt.name}")
            scope.define(stmt.name, Symbol(decl_type, mutable=stmt.mutable))
            return

        if isinstance(stmt, Assign):
            sym = scope.lookup(stmt.name)
            if sym.is_function:
                raise SemanticError(f"Cannot assign to function name: {stmt.name}")
            if not sym.mutable:
                raise SemanticError(f"Cannot assign to immutable binding: {stmt.name}")
            value_type = self._infer_expr(stmt.value, scope)
            self._require_same_type(sym.type, value_type, f"Type mismatch in assignment to {stmt.name}")
            return

        if isinstance(stmt, IfStmt):
            cond_type = self._infer_expr(stmt.condition, scope)
            self._require_type(cond_type, "bool", "If condition must be bool")
            self._analyze_block(stmt.then_block, scope, in_function)
            if stmt.else_block is not None:
                self._analyze_block(stmt.else_block, scope, in_function)
            return

        if isinstance(stmt, WhileStmt):
            cond_type = self._infer_expr(stmt.condition, scope)
            self._require_type(cond_type, "bool", "While condition must be bool")
            self._analyze_block(stmt.body, scope, in_function)
            return

        if isinstance(stmt, ForStmt):
            for_scope = Scope(scope)
            if stmt.init is not None:
                self._analyze_stmt(stmt.init, for_scope, in_function)
            cond_type = self._infer_expr(stmt.condition, for_scope)
            self._require_type(cond_type, "bool", "For condition must be bool")
            if stmt.increment is not None:
                self._analyze_stmt(stmt.increment, for_scope, in_function)
            self._analyze_block(stmt.body, for_scope, in_function)
            return

        if isinstance(stmt, ReturnStmt):
            if not in_function:
                raise SemanticError("Return is only allowed inside a function")
            if self.current_return is None:
                raise SemanticError("Internal error: missing function return type")
            if stmt.value is None:
                self._require_type(self.current_return, "void", "Return value required")
            else:
                value_type = self._infer_expr(stmt.value, scope)
                self._require_same_type(self.current_return, value_type, "Return type mismatch")
            return

        if isinstance(stmt, PrintStmt):
            self._infer_expr(stmt.value, scope)
            return

        if isinstance(stmt, ExprStmt):
            self._infer_expr(stmt.expr, scope)
            return

        if isinstance(stmt, Block):
            self._analyze_block(stmt, scope, in_function)
            return

        raise SemanticError(f"Unhandled statement type: {type(stmt).__name__}")

    def _infer_expr(self, expr: Expr, scope: Scope) -> TypeInfo:
        if isinstance(expr, Literal):
            t = self._type_of_literal(expr)
            self.expr_types[id(expr)] = t
            return t
        if isinstance(expr, VarRef):
            t = scope.lookup(expr.name).type
            self.expr_types[id(expr)] = t
            return t
        if isinstance(expr, Unary):
            t = self._infer_expr(expr.expr, scope)
            if expr.op == "-":
                if not t.is_numeric:
                    raise SemanticError("Unary - expects a numeric value")
                self.expr_types[id(expr)] = t
                return t
            if expr.op == "!":
                self._require_type(t, "bool", "Unary ! expects bool")
                self.expr_types[id(expr)] = BUILTINS["bool"]
                return BUILTINS["bool"]
            raise SemanticError(f"Unsupported unary operator: {expr.op}")
        if isinstance(expr, Binary):
            left = self._infer_expr(expr.left, scope)
            right = self._infer_expr(expr.right, scope)
            op = expr.op
            if op in {"+", "-", "*", "/", "%"}:
                self._require_same_type(left, right, f"Operands of {op} must have the same type")
                if not left.is_numeric:
                    raise SemanticError(f"Operands of {op} must be numeric")
                self.expr_types[id(expr)] = left
                return left
            if op in {"<", "<=", ">", ">="}:
                self._require_same_type(left, right, f"Operands of {op} must have the same type")
                if not left.is_numeric:
                    raise SemanticError(f"Operands of {op} must be numeric")
                self.expr_types[id(expr)] = BUILTINS["bool"]
                return BUILTINS["bool"]
            if op in {"==", "!="}:
                self._require_same_type(left, right, f"Operands of {op} must have the same type")
                self.expr_types[id(expr)] = BUILTINS["bool"]
                return BUILTINS["bool"]
            if op in {"&&", "||"}:
                self._require_type(left, "bool", f"Operands of {op} must be bool")
                self._require_type(right, "bool", f"Operands of {op} must be bool")
                self.expr_types[id(expr)] = BUILTINS["bool"]
                return BUILTINS["bool"]
            raise SemanticError(f"Unsupported binary operator: {op}")
        if isinstance(expr, Call):
            if expr.callee not in self.functions:
                raise SemanticError(f"Unknown function: {expr.callee}")
            info = self.functions[expr.callee]
            if len(expr.args) != len(info.param_types):
                raise SemanticError(
                    f"Function {expr.callee} expects {len(info.param_types)} argument(s), got {len(expr.args)}"
                )
            for arg, expected in zip(expr.args, info.param_types):
                actual = self._infer_expr(arg, scope)
                self._require_same_type(expected, actual, f"Argument type mismatch in call to {expr.callee}")
            self.expr_types[id(expr)] = info.return_type
            return info.return_type
        if isinstance(expr, Cast):
            src = self._infer_expr(expr.expr, scope)
            dst = self._type_of_ref(expr.target_type)
            if src.name == dst.name:
                self.expr_types[id(expr)] = dst
                return dst
            if src.is_numeric and dst.is_numeric:
                self.expr_types[id(expr)] = dst
                return dst
            if src.name == "bool" and dst.is_numeric:
                self.expr_types[id(expr)] = dst
                return dst
            if src.is_numeric and dst.name == "bool":
                self.expr_types[id(expr)] = dst
                return dst
            if src.name == "char" and dst.is_numeric:
                self.expr_types[id(expr)] = dst
                return dst
            if src.is_numeric and dst.name == "char":
                self.expr_types[id(expr)] = dst
                return dst
            raise SemanticError(f"Unsupported cast from {src.name} to {dst.name}")
        raise SemanticError(f"Unhandled expression type: {type(expr).__name__}")

    def _type_of_literal(self, lit: Literal) -> TypeInfo:
        return BUILTINS[lit.kind]

    def _type_of_ref(self, tref: TypeRef) -> TypeInfo:
        if tref.name not in BUILTINS:
            raise SemanticError(f"Unknown type: {tref.name}")
        return BUILTINS[tref.name]

    def _require_type(self, actual: TypeInfo, expected_name: str, message: str) -> None:
        if actual.name != expected_name:
            raise SemanticError(message)

    def _require_same_type(self, left: TypeInfo, right: TypeInfo, message: str) -> None:
        if left.name != right.name:
            raise SemanticError(message)
