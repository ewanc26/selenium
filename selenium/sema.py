"""Semantic analyser and type checker for Selenium.

Types are structural: a ``Type`` names a base (builtin, struct, union, enum
or the internal function marker), plus pointer depth, array dimensions and
an optional function signature. Aliases are expanded when they are resolved,
so the code generator only ever sees structural types.

The analyser runs in three steps: type declarations are registered in source
order, function signatures are registered (so forward calls resolve), then
every statement and expression is checked and typed. The resulting
``expr_types`` map is keyed by object identity for the code generator."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

from .ast import (
    AliasDecl,
    ArrayLit,
    Assign,
    Binary,
    Block,
    BraceLit,
    BreakStmt,
    Call,
    Cast,
    CompoundAssign,
    ContinueStmt,
    DoWhileStmt,
    EnumDecl,
    Expr,
    ExprStmt,
    ForStmt,
    FunctionDecl,
    GotoStmt,
    IfStmt,
    Index,
    Label,
    Literal,
    Member,
    Postfix,
    PrintStmt,
    Program,
    ReturnStmt,
    SizeofExpr,
    Stmt,
    StructDecl,
    SwitchStmt,
    Ternary,
    TypeRef,
    Unary,
    UnionDecl,
    VarDecl,
    VarRef,
    WhileStmt,
)


class SemanticError(Exception):
    """Raised when a program passes syntax but violates Selenium's type rules."""


INTEGER_BASES = frozenset(
    {"int", "uint", "short", "ushort", "long", "ulong", "schar", "uchar", "char"}
)
FLOAT_BASES = frozenset({"float", "single", "ldouble"})
PROMOTED_BASES = frozenset({"char", "short", "ushort", "schar", "uchar"})
BUILTIN_BASES = INTEGER_BASES | FLOAT_BASES | frozenset({"bool", "string", "void"})
FN_BASE = "<fn>"

READ_BUILTINS = {
    "read_int": "int",
    "read_float": "float",
    "read_bool": "bool",
    "read_char": "char",
}

PRINT_KINDS = {
    "int": "int",
    "short": "int",
    "ushort": "int",
    "schar": "int",
    "uchar": "int",
    "uint": "uint",
    "long": "long",
    "ulong": "ulong",
    "char": "char",
    "bool": "bool",
    "float": "float",
    "single": "float",
    "ldouble": "ldouble",
}


@dataclass(frozen=True)
class FnSig:
    """Parameter and return types of a function or function-pointer type."""

    params: Tuple["Type", ...]
    ret: "Type"


@dataclass(frozen=True)
class Type:
    """A structural type: base name, pointer depth, array dims (outermost first)."""

    base: str
    pointer: int = 0
    dims: Tuple[int, ...] = ()
    fn: Optional[FnSig] = None


INT = Type("int")
BOOL = Type("bool")
VOID = Type("void")
CHAR = Type("char")
FLOAT = Type("float")
STRING = Type("string")

LITERAL_TYPES = {
    "int": INT,
    "float": FLOAT,
    "char": CHAR,
    "bool": BOOL,
    "string": STRING,
}


@dataclass(slots=True)
class Symbol:
    type: Type
    mutable: bool
    kind: str = "var"  # var | const (enumerator) | fn | builtin


class Scope:
    """Lexical scope chain: names defined here or in parent scopes."""

    def __init__(self, parent: Optional["Scope"] = None):
        self.parent = parent
        self.symbols: Dict[str, Symbol] = {}

    def define(self, name: str, symbol: Symbol) -> None:
        """Bind a name in this scope. Rejects duplicates."""
        if name in self.symbols:
            raise SemanticError(f"Duplicate name: {name}")
        self.symbols[name] = symbol

    def lookup(self, name: str) -> Symbol:
        """Resolve a name by walking the scope chain upward."""
        scope: Optional[Scope] = self
        while scope is not None:
            if name in scope.symbols:
                return scope.symbols[name]
            scope = scope.parent
        raise SemanticError(f"Undefined name: {name}")


class SemanticAnalyzer:
    """Two-pass type checker and name resolver.

    Type declarations and function signatures are registered first; statements
    and expressions are then checked in order and typed into ``expr_types``."""

    def __init__(self):
        self.globals = Scope()
        self.type_kinds: Dict[str, str] = {}
        self.aliases: Dict[str, Type] = {}
        self.fields: Dict[str, Dict[str, Type]] = {}
        self.complete: set = set()
        self.current_return: Optional[Type] = None
        self.expr_types: Dict[int, Type] = {}
        self.loop_depth = 0
        self.switch_depth = 0
        self.used_builtins: set = set()
        for name, base in READ_BUILTINS.items():
            self.globals.define(
                name,
                Symbol(Type(FN_BASE, fn=FnSig((), Type(base))), False, "builtin"),
            )

    def analyze(self, program: Program) -> Program:
        """Run all passes over the program. Fills ``expr_types`` in place."""
        for item in program.items:
            if isinstance(item, (StructDecl, UnionDecl)):
                self._declare_record(item)
            elif isinstance(item, EnumDecl):
                self._declare_enum(item)
            elif isinstance(item, AliasDecl):
                self._declare_alias(item)
        for item in program.items:
            if isinstance(item, FunctionDecl):
                self._register_function(item)
        for item in program.items:
            if isinstance(item, (StructDecl, UnionDecl, EnumDecl, AliasDecl)):
                continue
            if isinstance(item, FunctionDecl):
                self._analyze_function(item)
            else:
                self._analyze_stmt(item, self.globals, in_function=False)
        return program

    # ── Declarations ──────────────────────────────────────────────

    def _check_type_name(self, name: str) -> None:
        if name in self.type_kinds or name in self.aliases or name in BUILTIN_BASES:
            raise SemanticError(f"Duplicate type name: {name}")

    def _declare_record(self, decl) -> None:
        self._check_type_name(decl.name)
        kind = "struct" if isinstance(decl, StructDecl) else "union"
        self.type_kinds[decl.name] = kind
        table: Dict[str, Type] = {}
        self.fields[decl.name] = table
        for field in decl.fields:
            if field.name in table:
                raise SemanticError(f"Duplicate field {field.name} in {decl.name}")
            ftype = self.resolve_type(field.type)
            if ftype.pointer == 0 and ftype.fn is None:
                if ftype.base == "void" and not ftype.dims:
                    raise SemanticError(f"Field {field.name} cannot have type void")
                if ftype.base in self.type_kinds and ftype.base not in self.complete:
                    raise SemanticError(
                        f"Field {field.name} has incomplete type {ftype.base}"
                    )
            table[field.name] = ftype
        self.complete.add(decl.name)

    def _declare_enum(self, decl: EnumDecl) -> None:
        self._check_type_name(decl.name)
        if not decl.enumerators:
            raise SemanticError(f"Enum {decl.name} needs at least one enumerator")
        self.type_kinds[decl.name] = "enum"
        self.complete.add(decl.name)
        enum_type = Type(decl.name)
        for name, _value in decl.enumerators:
            self.globals.define(name, Symbol(enum_type, False, "const"))

    def _declare_alias(self, decl: AliasDecl) -> None:
        self._check_type_name(decl.name)
        self.aliases[decl.name] = self.resolve_type(decl.target)

    def resolve_type(self, tref: TypeRef) -> Type:
        """Turn a written type into a structural Type, expanding aliases."""
        if tref.fn is not None:
            if tref.pointer or tref.dims:
                raise SemanticError(
                    "Function types cannot take pointer or array suffixes"
                )
            params = []
            for param in tref.fn.params:
                ptype = self.resolve_type(param)
                if ptype.dims or ptype.fn is not None or ptype == VOID:
                    raise SemanticError("Invalid function parameter type")
                params.append(ptype)
            ret = self.resolve_type(tref.fn.ret)
            if ret.dims or ret.fn is not None:
                raise SemanticError("Invalid function return type")
            return Type(FN_BASE, fn=FnSig(tuple(params), ret))
        if any(d <= 0 for d in tref.dims):
            raise SemanticError(f"Invalid array length in type {tref.name}")
        if tref.name in self.aliases:
            target = self.aliases[tref.name]
            if target.fn is not None:
                if tref.pointer or tref.dims:
                    raise SemanticError(
                        f"Function type alias {tref.name} cannot take suffixes"
                    )
                return target
            if tref.pointer and target.dims:
                raise SemanticError(f"Cannot add a pointer to array alias {tref.name}")
            return Type(
                target.base,
                target.pointer + tref.pointer,
                tuple(tref.dims) + target.dims,
            )
        if tref.name in BUILTIN_BASES or tref.name in self.type_kinds:
            return Type(tref.name, tref.pointer, tuple(tref.dims))
        raise SemanticError(f"Unknown type: {tref.name}")

    def _param_type(self, param) -> Type:
        ptype = self.resolve_type(param.type)
        if ptype.dims:
            raise SemanticError(f"Array parameters are not supported: {param.name}")
        if ptype == VOID:
            raise SemanticError(f"Parameter {param.name} cannot have type void")
        return ptype

    def _register_function(self, decl: FunctionDecl) -> None:
        if decl.name in self.globals.symbols:
            raise SemanticError(f"Duplicate function name: {decl.name}")
        params = tuple(self._param_type(p) for p in decl.params)
        ret = self.resolve_type(decl.return_type)
        if ret.dims or ret.fn is not None:
            raise SemanticError(
                f"Array and function return values are not supported: {decl.name}"
            )
        sig = FnSig(params, ret)
        self.globals.define(decl.name, Symbol(Type(FN_BASE, fn=sig), False, "fn"))

    def _analyze_function(self, decl: FunctionDecl) -> None:
        if decl.body is None:
            return
        sig = self.globals.lookup(decl.name).type.fn
        fn_scope = Scope(self.globals)
        for param, ptype in zip(decl.params, sig.params):
            fn_scope.define(param.name, Symbol(ptype, mutable=True))
        prev = self.current_return
        self.current_return = sig.ret
        self._analyze_block(decl.body, fn_scope, in_function=True)
        self.current_return = prev

    def _declare_var(self, decl: VarDecl, scope: Scope) -> None:
        vtype = self.resolve_type(decl.type)
        if vtype == VOID:
            raise SemanticError(f"Variable {decl.name} cannot have type void")
        self._check_init(vtype, decl.value, scope, decl.name)
        scope.define(decl.name, Symbol(vtype, mutable=decl.mutable))

    def _check_init(self, dst: Type, expr: Expr, scope: Scope, name: str) -> None:
        """Check an initialiser, descending into array and brace literals."""
        if isinstance(expr, ArrayLit):
            if not dst.dims:
                raise SemanticError(f"Array literal used for non-array in {name}")
            if len(expr.elements) > dst.dims[0]:
                raise SemanticError(
                    f"Too many elements in array initialiser for {name}"
                )
            elem = Type(dst.base, dst.pointer, dst.dims[1:], dst.fn)
            for element in expr.elements:
                self._check_init(elem, element, scope, name)
            self.expr_types[id(expr)] = dst
            return
        if isinstance(expr, BraceLit):
            if dst.dims or dst.pointer or dst.fn is not None:
                raise SemanticError(
                    f"Brace initialiser needs a struct or union: {name}"
                )
            if self.type_kinds.get(dst.base) not in ("struct", "union"):
                raise SemanticError(
                    f"Brace initialiser needs a struct or union: {name}"
                )
            members = list(self.fields[dst.base].values())
            limit = 1 if self.type_kinds[dst.base] == "union" else len(members)
            if len(expr.elements) > limit:
                raise SemanticError(f"Too many elements in initialiser for {name}")
            for element, member in zip(expr.elements, members):
                self._check_init(member, element, scope, name)
            self.expr_types[id(expr)] = dst
            return
        if dst.dims:
            raise SemanticError(f"Array {name} needs an array literal initialiser")
        self._assign_expr(dst, expr, scope, f"Type mismatch in declaration of {name}")

    # ── Statements ────────────────────────────────────────────────

    def _analyze_block(self, block: Block, scope: Scope, in_function: bool) -> None:
        child = Scope(scope)
        for stmt in block.statements:
            self._analyze_stmt(stmt, child, in_function)

    def _analyze_stmt(self, stmt: Stmt, scope: Scope, in_function: bool) -> None:
        if isinstance(stmt, VarDecl):
            self._declare_var(stmt, scope)
            return

        if isinstance(stmt, Assign):
            target_type = self._expr(stmt.target, scope)
            self._require_writable(stmt.target, scope, "Invalid assignment target")
            if target_type.dims:
                raise SemanticError("Cannot assign a whole array")
            self._assign_expr(
                target_type, stmt.value, scope, "Type mismatch in assignment"
            )
            return

        if isinstance(stmt, CompoundAssign):
            target_type = self._expr(stmt.target, scope)
            self._require_writable(stmt.target, scope, "Invalid assignment target")
            value_type = self._expr(stmt.value, scope)
            op = stmt.op
            common = self._unify(
                stmt.target,
                target_type,
                stmt.value,
                value_type,
                f"Operands of {op}= must have the same type",
            )
            if op in {"+", "-", "*", "/"}:
                if not self._is_arith(common):
                    raise SemanticError(f"Operands of {op}= must be numeric")
            elif not self._is_integer(common):
                raise SemanticError(f"Operands of {op}= must be integers")
            if common != target_type and not self._compatible(target_type, common):
                raise SemanticError(f"Type mismatch in {op}= assignment")
            return

        if isinstance(stmt, IfStmt):
            self._require_bool(
                self._expr(stmt.condition, scope), "If condition must be bool"
            )
            self._analyze_block(stmt.then_block, scope, in_function)
            if stmt.else_block is not None:
                self._analyze_block(stmt.else_block, scope, in_function)
            return

        if isinstance(stmt, WhileStmt):
            self._require_bool(
                self._expr(stmt.condition, scope), "While condition must be bool"
            )
            self.loop_depth += 1
            self._analyze_block(stmt.body, scope, in_function)
            self.loop_depth -= 1
            return

        if isinstance(stmt, DoWhileStmt):
            self.loop_depth += 1
            self._analyze_block(stmt.body, scope, in_function)
            self.loop_depth -= 1
            self._require_bool(
                self._expr(stmt.condition, scope), "Do-while condition must be bool"
            )
            return

        if isinstance(stmt, GotoStmt):
            return

        if isinstance(stmt, Label):
            self._analyze_block(stmt.body, scope, in_function)
            return

        if isinstance(stmt, ForStmt):
            for_scope = Scope(scope)
            init = stmt.init
            if isinstance(init, VarDecl):
                if init.qualifiers:
                    raise SemanticError(
                        "Qualifiers are not allowed in for initialisers"
                    )
                if self.resolve_type(init.type).dims:
                    raise SemanticError(
                        "Array declarations are not allowed in for initialisers"
                    )
                self._declare_var(init, for_scope)
            elif init is not None:
                self._analyze_stmt(init, for_scope, in_function)
            self._require_bool(
                self._expr(stmt.condition, for_scope), "For condition must be bool"
            )
            if stmt.increment is not None:
                self._analyze_stmt(stmt.increment, for_scope, in_function)
            self.loop_depth += 1
            self._analyze_block(stmt.body, for_scope, in_function)
            self.loop_depth -= 1
            return

        if isinstance(stmt, SwitchStmt):
            switch_type = self._expr(stmt.expr, scope)
            if not self._is_integer(switch_type):
                raise SemanticError("Switch expression must be an integer")
            seen_values = set()
            self.switch_depth += 1
            for case in stmt.cases:
                case_type = self._expr(case.value, scope)
                if not self._adapt(case.value, switch_type) and not self._compatible(
                    switch_type, case_type
                ):
                    raise SemanticError("Case value must match the switch type")
                if isinstance(case.value, Literal) and case.value.kind in (
                    "int",
                    "char",
                ):
                    val = case.value.value
                    key = ord(val) if case.value.kind == "char" else val
                    if key in seen_values:
                        raise SemanticError(f"Duplicate case value: {val}")
                    seen_values.add(key)
                self._analyze_block(case.body, scope, in_function)
            if stmt.default is not None:
                self._analyze_block(stmt.default, scope, in_function)
            self.switch_depth -= 1
            return

        if isinstance(stmt, ReturnStmt):
            if not in_function:
                raise SemanticError("Return is only allowed inside a function")
            if self.current_return is None:
                raise SemanticError("Internal error: missing function return type")
            if stmt.value is None:
                if self.current_return != VOID:
                    raise SemanticError("Return value required")
            else:
                if self.current_return == VOID:
                    raise SemanticError("A void function cannot return a value")
                self._assign_expr(
                    self.current_return, stmt.value, scope, "Return type mismatch"
                )
            return

        if isinstance(stmt, BreakStmt):
            if self.loop_depth == 0 and self.switch_depth == 0:
                raise SemanticError("Break is only allowed inside loops or switch")
            return

        if isinstance(stmt, ContinueStmt):
            if self.loop_depth == 0:
                raise SemanticError("Continue is only allowed inside loops")
            return

        if isinstance(stmt, PrintStmt):
            self.used_builtins.add(self.print_kind(self._expr(stmt.value, scope)))
            return

        if isinstance(stmt, ExprStmt):
            self._expr(stmt.expr, scope)
            return

        if isinstance(stmt, Block):
            self._analyze_block(stmt, scope, in_function)
            return

        raise SemanticError(f"Unhandled statement type: {type(stmt).__name__}")

    # ── Expressions ───────────────────────────────────────────────

    def _expr(self, expr: Expr, scope: Scope) -> Type:
        """Type an expression, record it in ``expr_types`` and return it."""
        t = self._infer(expr, scope)
        self.expr_types[id(expr)] = t
        return t

    def _infer(self, expr: Expr, scope: Scope) -> Type:
        if isinstance(expr, Literal):
            try:
                return LITERAL_TYPES[expr.kind]
            except KeyError as exc:
                raise SemanticError(f"Unknown literal kind: {expr.kind}") from exc
        if isinstance(expr, VarRef):
            sym = scope.lookup(expr.name)
            if sym.kind == "builtin":
                self.used_builtins.add(expr.name)
            return sym.type
        if isinstance(expr, (ArrayLit, BraceLit)):
            raise SemanticError(
                "Array and brace literals are only allowed as initialisers"
            )
        if isinstance(expr, Index):
            target = self._expr(expr.target, scope)
            index = self._expr(expr.index, scope)
            if not self._adapt(expr.index, INT) and not self._is_integer(index):
                raise SemanticError("Array index must be an integer")
            if target.dims:
                return Type(target.base, target.pointer, target.dims[1:], target.fn)
            if target.pointer:
                return Type(target.base, target.pointer - 1)
            if target.base == "string":
                return CHAR
            raise SemanticError("Only arrays and pointers can be indexed")
        if isinstance(expr, Unary):
            return self._unary(expr, scope)
        if isinstance(expr, Postfix):
            t = self._expr(expr.expr, scope)
            self._require_writable(
                expr.expr, scope, "Increment target must be a variable"
            )
            if not self._is_arith(t):
                raise SemanticError(f"Operator {expr.op} expects a numeric value")
            return t
        if isinstance(expr, Ternary):
            self._require_bool(
                self._expr(expr.condition, scope), "Ternary condition must be bool"
            )
            then_t = self._expr(expr.then_expr, scope)
            else_t = self._expr(expr.else_expr, scope)
            return self._unify(
                expr.then_expr,
                then_t,
                expr.else_expr,
                else_t,
                "Ternary branches must have the same type",
            )
        if isinstance(expr, Binary):
            return self._binary(expr, scope)
        if isinstance(expr, Call):
            callee = self._expr(expr.callee, scope)
            if callee.fn is None or callee.pointer or callee.dims:
                raise SemanticError("Cannot call a value that is not a function")
            sig = callee.fn
            if len(expr.args) != len(sig.params):
                raise SemanticError(
                    f"Function expects {len(sig.params)} argument(s), got {len(expr.args)}"
                )
            for arg, expected in zip(expr.args, sig.params):
                self._assign_expr(
                    expected, arg, scope, "Argument type mismatch in call"
                )
            return sig.ret
        if isinstance(expr, Cast):
            target = self.resolve_type(expr.target_type)
            source = self._expr(expr.expr, scope)
            if target == VOID:
                return VOID
            if not self._is_scalar(source) or not self._is_scalar(target):
                raise SemanticError(
                    f"Unsupported cast from {source.base} to {target.base}"
                )
            return target
        if isinstance(expr, SizeofExpr):
            if expr.target_type is not None:
                self.resolve_type(expr.target_type)
            else:
                self._expr(expr.expr, scope)
            return INT
        if isinstance(expr, Member):
            return self._member(expr, scope)
        raise SemanticError(f"Unhandled expression type: {type(expr).__name__}")

    def _unary(self, expr: Unary, scope: Scope) -> Type:
        if expr.op == "&":
            inner = self._expr(expr.expr, scope)
            self._require_writable(
                expr.expr, scope, "Cannot take the address of a read-only location"
            )
            if inner.dims or inner.fn is not None:
                raise SemanticError("Cannot take the address of an array or function")
            return Type(inner.base, inner.pointer + 1)
        if expr.op == "*":
            inner = self._expr(expr.expr, scope)
            if inner.fn is not None:
                raise SemanticError("Cannot dereference a function")
            if inner.dims:
                return Type(inner.base, inner.pointer, inner.dims[1:])
            if inner.pointer:
                return Type(inner.base, inner.pointer - 1)
            if inner.base == "string" and not inner.pointer:
                return CHAR
            raise SemanticError("Cannot dereference a non-pointer value")
        inner = self._expr(expr.expr, scope)
        if expr.op == "-":
            if not self._is_arith(inner):
                raise SemanticError("Unary - expects a numeric value")
            return self._promote(inner)
        if expr.op == "~":
            if not self._is_integer(inner):
                raise SemanticError("Unary ~ expects an integer")
            return self._promote(inner)
        if expr.op == "!":
            self._require_bool(inner, "Unary ! expects bool")
            return BOOL
        if expr.op in {"++", "--"}:
            self._require_writable(
                expr.expr, scope, "Increment target must be a variable"
            )
            if not self._is_arith(inner):
                raise SemanticError(f"Unary {expr.op} expects a numeric value")
            return inner
        raise SemanticError(f"Unsupported unary operator: {expr.op}")

    def _binary(self, expr: Binary, scope: Scope) -> Type:
        op = expr.op
        if op == ",":
            self._expr(expr.left, scope)
            return self._expr(expr.right, scope)
        left = self._expr(expr.left, scope)
        right = self._expr(expr.right, scope)
        if op in {"&&", "||"}:
            self._require_bool(left, f"Operands of {op} must be bool")
            self._require_bool(right, f"Operands of {op} must be bool")
            return BOOL
        common = self._unify(
            expr.left,
            left,
            expr.right,
            right,
            f"Operands of {op} must have the same type",
        )
        if op in {"==", "!="}:
            if not self._is_scalar(common):
                raise SemanticError(f"Operands of {op} must be scalar values")
            return BOOL
        if op in {"<", "<=", ">", ">="}:
            if not self._is_arith(common):
                raise SemanticError(f"Operands of {op} must be numeric")
            return BOOL
        if op in {"+", "-", "*", "/"}:
            if not self._is_arith(common):
                raise SemanticError(f"Operands of {op} must be numeric")
            return self._promote(common)
        if op in {"%", "<<", ">>", "&", "|", "^"}:
            if not self._is_integer(common):
                raise SemanticError(f"Operands of {op} must be integers")
            return self._promote(common)
        raise SemanticError(f"Unsupported binary operator: {op}")

    def _member(self, expr: Member, scope: Scope) -> Type:
        target = self._expr(expr.target, scope)
        if target.fn is not None or target.dims:
            raise SemanticError(f"Cannot access field {expr.name} of this value")
        if expr.arrow:
            if target.pointer != 1:
                raise SemanticError("'->' needs a pointer to a struct or union")
            owner = Type(target.base)
        else:
            if target.pointer:
                raise SemanticError(
                    "'.' needs a struct or union value; use '->' for pointers"
                )
            owner = target
        if self.type_kinds.get(owner.base) not in ("struct", "union"):
            raise SemanticError(f"Field access on a non-struct value: {owner.base}")
        field = self.fields[owner.base].get(expr.name)
        if field is None:
            raise SemanticError(f"Unknown field {expr.name} in {owner.base}")
        return field

    # ── Type relations ────────────────────────────────────────────

    def _is_enum(self, t: Type) -> bool:
        return (
            t.fn is None
            and t.pointer == 0
            and not t.dims
            and self.type_kinds.get(t.base) == "enum"
        )

    def _is_integer(self, t: Type) -> bool:
        if t.fn is not None or t.pointer or t.dims:
            return False
        return t.base in INTEGER_BASES or self._is_enum(t)

    def _is_float(self, t: Type) -> bool:
        return t.fn is None and t.pointer == 0 and not t.dims and t.base in FLOAT_BASES

    def _is_arith(self, t: Type) -> bool:
        return self._is_integer(t) or self._is_float(t)

    def _is_scalar(self, t: Type) -> bool:
        if t.fn is not None or t.dims:
            return False
        if t.pointer:
            return True
        return (
            t.base in INTEGER_BASES
            or t.base in FLOAT_BASES
            or t.base in ("bool", "string")
            or self._is_enum(t)
        )

    def _promote(self, t: Type) -> Type:
        if self._is_enum(t):
            return INT
        if t.pointer == 0 and not t.dims and t.fn is None and t.base in PROMOTED_BASES:
            return INT
        return t

    def _compatible(self, dst: Type, src: Type) -> bool:
        if dst == src:
            return True
        return (self._is_enum(dst) and src == INT) or (
            dst == INT and self._is_enum(src)
        )

    def _adapt(self, expr: Expr, target: Type) -> bool:
        """Give an unadapted literal the type ``target`` if it fits."""
        if (
            isinstance(expr, Unary)
            and expr.op == "-"
            and isinstance(expr.expr, Literal)
        ):
            if not self._adapt(expr.expr, target):
                return False
            self.expr_types[id(expr)] = target
            return True
        if not isinstance(expr, Literal):
            return False
        if expr.kind in ("int", "char"):
            ok = self._is_integer(target)
        elif expr.kind == "float":
            ok = self._is_float(target)
        else:
            ok = False
        if ok:
            self.expr_types[id(expr)] = target
        return ok

    def _unify(
        self, lexpr: Expr, lt: Type, rexpr: Expr, rt: Type, message: str
    ) -> Type:
        """Find the common type of two operands, adapting literals if needed."""
        if lt == rt:
            return lt
        if self._adapt(rexpr, lt):
            return lt
        if self._adapt(lexpr, rt):
            return rt
        if self._compatible(lt, rt):
            return rt if self._is_enum(lt) else lt
        raise SemanticError(message)

    def _assign_expr(self, dst: Type, expr: Expr, scope: Scope, message: str) -> None:
        src = self._expr(expr, scope)
        if self._adapt(expr, dst):
            return
        if not self._compatible(dst, src):
            raise SemanticError(message)

    def _require_bool(self, t: Type, message: str) -> None:
        if t != BOOL:
            raise SemanticError(message)

    def _require_writable(self, expr: Expr, scope: Scope, message: str) -> None:
        """Raise unless ``expr`` names a location that may be written."""
        if isinstance(expr, VarRef):
            sym = scope.lookup(expr.name)
            if sym.kind != "var":
                raise SemanticError(f"{message}: {expr.name} is not a variable")
            if not sym.mutable:
                raise SemanticError(f"Cannot assign to immutable binding: {expr.name}")
            return
        if isinstance(expr, Index):
            target = self.expr_types[id(expr.target)]
            if target.dims:
                self._require_writable(expr.target, scope, message)
                return
            if target.base == "string" and not target.pointer:
                raise SemanticError("Strings are read-only")
            if target.pointer:
                return
            raise SemanticError(message)
        if isinstance(expr, Member):
            if expr.arrow:
                return
            self._require_writable(expr.target, scope, message)
            return
        if isinstance(expr, Unary) and expr.op == "*":
            inner = self.expr_types[id(expr.expr)]
            if inner.base == "string" and not inner.pointer and not inner.dims:
                raise SemanticError("Strings are read-only")
            return
        raise SemanticError(message)

    # ── Code generator support ────────────────────────────────────

    def print_kind(self, t: Type) -> str:
        """Name of the print helper used for values of type ``t``."""
        if t.fn is not None or t.dims or t.pointer:
            raise SemanticError("Cannot print this type")
        if t.base == "string":
            return "string"
        if self._is_enum(t):
            return "int"
        try:
            return PRINT_KINDS[t.base]
        except KeyError as exc:
            raise SemanticError(f"Cannot print type {t.base}") from exc
