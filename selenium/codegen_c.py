"""C code generator for Selenium.

Walks the analysed AST and emits C23-compatible source. Type declarations
come first, then prototypes, globals, function definitions and ``main``.
Declarators are built recursively from structural types, so function
pointers, pointer-to-const bindings and multi-dimensional arrays all come
out in standard C syntax. I/O builtins compile to static helpers emitted in
the prelude, only when the program uses them."""

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
    Unary,
    UnionDecl,
    VarDecl,
    VarRef,
    WhileStmt,
)
from .sema import READ_BUILTINS, Type


class CodegenError(Exception):
    """Raised when an AST node cannot be translated to C."""


C_BASE = {
    "int": "int",
    "uint": "unsigned int",
    "short": "short",
    "ushort": "unsigned short",
    "long": "long",
    "ulong": "unsigned long",
    "schar": "signed char",
    "uchar": "unsigned char",
    "char": "char",
    "bool": "_Bool",
    "float": "double",
    "single": "float",
    "ldouble": "long double",
    "void": "void",
    "string": "const char",
}

# print kind -> (C parameter type, printf conversion)
PRINT_HELPERS = {
    "int": ("int", "%d"),
    "uint": ("unsigned int", "%u"),
    "long": ("long", "%ld"),
    "ulong": ("unsigned long", "%lu"),
    "char": ("char", "%c"),
    "float": ("double", "%g"),
    "ldouble": ("long double", "%Lg"),
    "string": ("const char *", "%s"),
}

READ_HELPERS = {
    "read_int": 'static int selenium_read_int(void) { int x; scanf("%d", &x); return x; }',
    "read_float": 'static double selenium_read_float(void) { double x; scanf("%lf", &x); return x; }',
    "read_bool": 'static _Bool selenium_read_bool(void) { int x; scanf("%d", &x); return x; }',
    "read_char": 'static char selenium_read_char(void) { char x; scanf(" %c", &x); return x; }',
}

TYPE_DECLS = (StructDecl, UnionDecl, EnumDecl, AliasDecl)


@dataclass(slots=True)
class CodegenContext:
    expr_types: Dict[int, Type]
    used_builtins: set
    analyzer: object  # selenium.sema.SemanticAnalyzer


class CCodeGenerator:
    """Walk the analysed AST and emit C source code."""

    def __init__(self, program: Program, context: CodegenContext):
        self.program = program
        self.ctx = context
        self.lines: List[str] = []
        self.indent = 0

    def generate(self) -> str:
        """Produce the full C source."""
        self.lines = []
        self.indent = 0
        items = self.program.items
        self._emit_prelude(self.ctx.used_builtins)
        for item in items:
            if isinstance(item, (StructDecl, UnionDecl)):
                self._emit_record(item)
            elif isinstance(item, EnumDecl):
                self._emit_enum(item)
        functions = [item for item in items if isinstance(item, FunctionDecl)]
        for fn in functions:
            self._emit_prototype(fn)
        self._writeline("")
        self._dynamic_globals = set()
        for item in items:
            if not isinstance(item, VarDecl):
                continue
            if self._is_static_init(item.value):
                self._writeline(self._var_line(item))
                continue
            if isinstance(item.value, (ArrayLit, BraceLit)):
                raise CodegenError(
                    f"Global {item.name} needs constant elements in its initialiser"
                )
            # C needs a constant initialiser at file scope, so the value is
            # assigned in main at the point where the declaration appears.
            self._dynamic_globals.add(id(item))
            self._writeline(self._var_text(item, initialise=False) + ";")
        self._writeline("")
        for fn in functions:
            if fn.body is not None:
                self._emit_function(fn)
                self._writeline("")
        statements = [
            item for item in items if not isinstance(item, (FunctionDecl,) + TYPE_DECLS)
        ]
        self._emit_main(statements)
        return "\n".join(self.lines) + "\n"

    def _is_static_init(self, expr: Expr) -> bool:
        """True when ``expr`` is a constant expression valid at file scope."""
        if isinstance(expr, (Literal, SizeofExpr)):
            return True
        if isinstance(expr, Unary):
            return expr.op in ("-", "+", "~", "!") and self._is_static_init(expr.expr)
        if isinstance(expr, Cast):
            return self._is_static_init(expr.expr)
        if isinstance(expr, Binary):
            return (
                expr.op != ","
                and self._is_static_init(expr.left)
                and self._is_static_init(expr.right)
            )
        if isinstance(expr, Ternary):
            return all(
                self._is_static_init(e)
                for e in (expr.condition, expr.then_expr, expr.else_expr)
            )
        if isinstance(expr, (ArrayLit, BraceLit)):
            return all(self._is_static_init(e) for e in expr.elements)
        return False

    # ── Prelude ───────────────────────────────────────────────────

    def _emit_prelude(self, used: set) -> None:
        self._writeline("#include <stdbool.h>")
        self._writeline("#include <stdio.h>")
        self._writeline("#include <stdlib.h>")
        self._writeline("")
        for kind, (ctype, fmt) in PRINT_HELPERS.items():
            if kind in used:
                self._writeline(
                    f'static void selenium_print_{kind}({ctype} value) {{ printf("{fmt}\\n", value); }}'
                )
        if "bool" in used:
            self._writeline(
                'static void selenium_print_bool(_Bool value) { printf("%s\\n", value ? "true" : "false"); }'
            )
        for name, helper in READ_HELPERS.items():
            if name in used:
                self._writeline(helper)
        self._writeline("")

    # ── Type declarations ─────────────────────────────────────────

    def _emit_record(self, decl) -> None:
        kind = "struct" if isinstance(decl, StructDecl) else "union"
        self._writeline(f"{kind} {decl.name} {{")
        self.indent += 1
        for field in decl.fields:
            ftype = self._resolve(field.type)
            self._writeline(self._decl(ftype, field.name) + ";")
        self.indent -= 1
        self._writeline("};")
        self._writeline("")

    def _emit_enum(self, decl: EnumDecl) -> None:
        items = [
            name if value is None else f"{name} = {value}"
            for name, value in decl.enumerators
        ]
        self._writeline(f"enum {decl.name} {{ {', '.join(items)} }};")
        self._writeline("")

    # ── Functions ─────────────────────────────────────────────────

    def _signature_text(self, fn: FunctionDecl) -> str:
        params = (
            ", ".join(self._decl(self._resolve(p.type), p.name) for p in fn.params)
            or "void"
        )
        ret = self._resolve(fn.return_type)
        return self._decl(ret, f"{fn.name}({params})")

    def _emit_prototype(self, fn: FunctionDecl) -> None:
        prefix = "static " if fn.body is not None else ""
        self._writeline(f"{prefix}{self._signature_text(fn)};")

    def _emit_function(self, fn: FunctionDecl) -> None:
        self._writeline(f"static {self._signature_text(fn)} {{")
        self.indent += 1
        self._emit_statements(fn.body.statements)
        self.indent -= 1
        self._writeline("}")

    def _emit_main(self, items: List[object]) -> None:
        self._writeline("int main(void) {")
        self.indent += 1
        for item in items:
            if isinstance(item, VarDecl):
                if id(item) in self._dynamic_globals:
                    self._writeline(f"{item.name} = {self._expr(item.value)};")
            else:
                self._emit_item(item)
        self._writeline("return 0;")
        self.indent -= 1
        self._writeline("}")

    # ── Statements ────────────────────────────────────────────────

    def _emit_statements(self, statements: List[Stmt]) -> None:
        for stmt in statements:
            self._emit_item(stmt)

    def _emit_item(self, item: object) -> None:
        if isinstance(item, VarDecl):
            self._writeline(self._var_line(item))
        elif isinstance(item, Assign):
            self._writeline(f"{self._expr(item.target)} = {self._expr(item.value)};")
        elif isinstance(item, CompoundAssign):
            self._writeline(
                f"{self._expr(item.target)} {item.op}= {self._expr(item.value)};"
            )
        elif isinstance(item, IfStmt):
            self._emit_if(item)
        elif isinstance(item, WhileStmt):
            self._writeline(f"while ({self._expr(item.condition)}) {{")
            self._emit_body(item.body)
            self._writeline("}")
        elif isinstance(item, DoWhileStmt):
            self._writeline("do {")
            self._emit_body(item.body)
            self._writeline(f"}} while ({self._expr(item.condition)});")
        elif isinstance(item, GotoStmt):
            self._writeline(f"goto {item.label};")
        elif isinstance(item, Label):
            self._writeline(f"{item.name}:;")
            self._writeline("{")
            self._emit_body(item.body)
            self._writeline("}")
        elif isinstance(item, SwitchStmt):
            self._emit_switch(item)
        elif isinstance(item, ForStmt):
            self._emit_for(item)
        elif isinstance(item, ReturnStmt):
            if item.value is None:
                self._writeline("return;")
            else:
                self._writeline(f"return {self._expr(item.value)};")
        elif isinstance(item, BreakStmt):
            self._writeline("break;")
        elif isinstance(item, ContinueStmt):
            self._writeline("continue;")
        elif isinstance(item, PrintStmt):
            kind = self.ctx.analyzer.print_kind(self._type_of(item.value))
            self._writeline(f"selenium_print_{kind}({self._expr(item.value)});")
        elif isinstance(item, ExprStmt):
            self._writeline(f"{self._expr(item.expr)};")
        elif isinstance(item, Block):
            self._writeline("{")
            self._emit_body(item)
            self._writeline("}")
        elif isinstance(item, FunctionDecl) or isinstance(item, TYPE_DECLS):
            return
        else:
            raise CodegenError(f"Unhandled item: {type(item).__name__}")

    def _emit_body(self, block: Block) -> None:
        self.indent += 1
        self._emit_statements(block.statements)
        self.indent -= 1

    def _emit_if(self, stmt: IfStmt) -> None:
        self._writeline(f"if ({self._expr(stmt.condition)}) {{")
        self._emit_body(stmt.then_block)
        if stmt.else_block is None:
            self._writeline("}")
        else:
            self._writeline("} else {")
            self._emit_body(stmt.else_block)
            self._writeline("}")

    def _emit_switch(self, stmt: SwitchStmt) -> None:
        self._writeline(f"switch ({self._expr(stmt.expr)}) {{")
        self.indent += 1
        for case in stmt.cases:
            self._writeline(f"case {self._expr(case.value)}:")
            self._emit_body(case.body)
        if stmt.default is not None:
            self._writeline("default:")
            self._emit_body(stmt.default)
        self.indent -= 1
        self._writeline("}")

    def _emit_for(self, stmt: ForStmt) -> None:
        init = "" if stmt.init is None else self._simple_text(stmt.init)
        cond = self._expr(stmt.condition)
        inc = "" if stmt.increment is None else self._simple_text(stmt.increment)
        self._writeline(f"for ({init}; {cond}; {inc}) {{")
        self._emit_body(stmt.body)
        self._writeline("}")

    def _simple_text(self, stmt) -> str:
        """A for-loop clause: declaration, assignment or expression, without ';'."""
        if isinstance(stmt, VarDecl):
            return self._var_text(stmt)
        if isinstance(stmt, Assign):
            return f"{self._expr(stmt.target)} = {self._expr(stmt.value)}"
        if isinstance(stmt, CompoundAssign):
            return f"{self._expr(stmt.target)} {stmt.op}= {self._expr(stmt.value)}"
        if isinstance(stmt, ExprStmt):
            return self._expr(stmt.expr)
        raise CodegenError(f"Unsupported for-loop clause: {type(stmt).__name__}")

    # ── Declarations ──────────────────────────────────────────────

    def _var_text(self, decl: VarDecl, initialise: bool = True) -> str:
        vtype = self._resolve(decl.type)
        storage = ""
        if "pinned" in decl.qualifiers:
            storage += "static "
        if "erratic" in decl.qualifiers:
            storage += "volatile "
        # A deferred global cannot be const in C, since it is assigned later.
        const = initialise and not decl.mutable
        text = f"{storage}{self._decl(vtype, decl.name, const=const)}"
        if initialise:
            text += f" = {self._expr(decl.value)}"
        return text

    def _var_line(self, decl: VarDecl) -> str:
        return self._var_text(decl) + ";"

    def _decl(self, t: Type, name: str = "", const: bool = False) -> str:
        """Render a C declarator for structural type ``t`` around ``name``."""
        if t.fn is not None:
            params = ", ".join(self._decl(p) for p in t.fn.params) or "void"
            star = "*" + (" const" if const else "")
            inner = f"({star}{' ' + name if name else ''})"
            return self._decl(t.fn.ret, f"{inner}({params})")
        base, extra = self._c_base(t)
        count = t.pointer + extra
        if const:
            if count:
                stars = "*" * count + " const"
            else:
                base = "const " + base
                stars = ""
        else:
            stars = "*" * count
        text = stars
        if name:
            if not text:
                text = name
            elif text.endswith("*"):
                text = text + name
            else:
                text = f"{text} {name}"
        text += "".join(f"[{n}]" for n in t.dims)
        return f"{base} {text}" if text else base

    def _c_base(self, t: Type) -> Tuple[str, int]:
        if t.base in C_BASE:
            return C_BASE[t.base], (1 if t.base == "string" else 0)
        kind = self.ctx.analyzer.type_kinds.get(t.base)
        if kind is None:
            raise CodegenError(f"Unsupported C type: {t.base}")
        return f"{kind} {t.base}", 0

    def _resolve(self, tref) -> Type:
        return self.ctx.analyzer.resolve_type(tref)

    # ── Expressions ───────────────────────────────────────────────

    def _expr(self, expr: Expr) -> str:
        if isinstance(expr, Literal):
            if expr.kind == "string":
                return _c_string(expr.value)
            if expr.kind == "char":
                return _c_char(expr.value)
            if expr.kind == "bool":
                return "true" if expr.value else "false"
            if expr.kind == "float":
                return repr(float(expr.value))
            return str(expr.value)
        if isinstance(expr, VarRef):
            if expr.name in READ_BUILTINS:
                return f"selenium_{expr.name}"
            return expr.name
        if isinstance(expr, (ArrayLit, BraceLit)):
            return "{" + ", ".join(self._expr(e) for e in expr.elements) + "}"
        if isinstance(expr, Index):
            return f"{self._expr(expr.target)}[{self._expr(expr.index)}]"
        if isinstance(expr, Member):
            op = "->" if expr.arrow else "."
            return f"{self._expr(expr.target)}{op}{expr.name}"
        if isinstance(expr, Cast):
            target = self._decl(self._resolve(expr.target_type))
            return f"(({target})({self._expr(expr.expr)}))"
        if isinstance(expr, Call):
            args = ", ".join(self._expr(arg) for arg in expr.args)
            return f"{self._expr(expr.callee)}({args})"
        if isinstance(expr, Unary):
            return f"({expr.op}{self._expr(expr.expr)})"
        if isinstance(expr, Postfix):
            return f"({self._expr(expr.expr)}{expr.op})"
        if isinstance(expr, SizeofExpr):
            if expr.target_type is not None:
                return f"((int)sizeof({self._decl(self._resolve(expr.target_type))}))"
            return f"((int)sizeof({self._expr(expr.expr)}))"
        if isinstance(expr, Ternary):
            return (
                f"({self._expr(expr.condition)} ? {self._expr(expr.then_expr)}"
                f" : {self._expr(expr.else_expr)})"
            )
        if isinstance(expr, Binary):
            return f"({self._expr(expr.left)} {expr.op} {self._expr(expr.right)})"
        raise CodegenError(f"Unhandled expr: {type(expr).__name__}")

    def _type_of(self, expr: Expr) -> Type:
        try:
            return self.ctx.expr_types[id(expr)]
        except KeyError as exc:
            raise CodegenError(
                f"Missing inferred type for {type(expr).__name__}"
            ) from exc

    # ── Output helpers ────────────────────────────────────────────

    def _writeline(self, text: str) -> None:
        self.lines.append("    " * self.indent + text)


def _c_string(value: str) -> str:
    out = []
    for ch in value:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\r":
            out.append("\\r")
        elif ord(ch) < 32 or ord(ch) >= 127:
            out.append(f"\\{ord(ch):03o}")
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def _c_char(value: str) -> str:
    if value == "\\":
        body = "\\\\"
    elif value == "'":
        body = "\\'"
    elif 32 <= ord(value) < 127:
        body = value
    else:
        body = f"\\{ord(value):03o}"
    return f"'{body}'"
