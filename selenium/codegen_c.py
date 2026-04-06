from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, List

from .ast import (
    Assign,
    Binary,
    Block,
    BreakStmt,
    Call,
    Case,
    Cast,
    ContinueStmt,
    Expr,
    ExprStmt,
    ForStmt,
    FunctionDecl,
    IfStmt,
    Literal,
    PrintStmt,
    Program,
    ReturnStmt,
    Stmt,
    SwitchStmt,
    Ternary,
    TopLevel,
    Unary,
    VarDecl,
    VarRef,
    WhileStmt,
)
from .sema import TypeInfo


class CodegenError(Exception):
    pass


@dataclass(slots=True)
class CodegenContext:
    expr_types: Dict[int, TypeInfo]


class CCodeGenerator:
    def __init__(self, program: Program, context: CodegenContext):
        self.program = program
        self.ctx = context
        self.lines: List[str] = []
        self.indent = 0

    def generate(self) -> str:
        self.lines = []
        self.indent = 0
        self._emit_prelude()
        functions = [item for item in self.program.items if isinstance(item, FunctionDecl)]
        globals_ = [item for item in self.program.items if isinstance(item, VarDecl)]
        statements = [item for item in self.program.items if not isinstance(item, (FunctionDecl, VarDecl))]
        for var in globals_:
            self._emit_global_var(var)
            self._writeline("")
        for fn in functions:
            self._emit_function(fn)
            self._writeline("")
        self._emit_main(statements)
        return "\n".join(self.lines) + "\n"

    def _emit_prelude(self) -> None:
        self._writeline("#include <stdbool.h>")
        self._writeline("#include <stdio.h>")
        self._writeline("#include <stdlib.h>")
        self._writeline("")
        self._writeline("static void selenium_print_int(int value) { printf(\"%d\\n\", value); }")
        self._writeline("static void selenium_print_float(double value) { printf(\"%g\\n\", value); }")
        self._writeline(
            "static void selenium_print_bool(_Bool value) { printf(\"%s\\n\", value ? \"true\" : \"false\"); }"
        )
        self._writeline("static void selenium_print_char(char value) { printf(\"%c\\n\", value); }")
        self._writeline("static void selenium_print_string(const char *value) { printf(\"%s\\n\", value); }")
        self._writeline("")

    def _emit_function(self, fn: FunctionDecl) -> None:
        ret = self._c_type(fn.return_type.name)
        params = ", ".join(f"{self._c_type(p.type.name)} {p.name}" for p in fn.params)
        if not params:
            params = "void"
        self._writeline(f"static {ret} {fn.name}({params}) {{")
        self.indent += 1
        self._emit_statements(fn.body.statements)
        self.indent -= 1
        self._writeline("}")

    def _emit_main(self, items: List[TopLevel]) -> None:
        self._writeline("int main(void) {")
        self.indent += 1
        for item in items:
            self._emit_item(item)
        self._writeline("return 0;")
        self.indent -= 1
        self._writeline("}")

    def _emit_item(self, item: object) -> None:
        if isinstance(item, VarDecl):
            self._emit_vardecl(item)
        elif isinstance(item, Assign):
            self._writeline(f"{item.name} = {self._expr(item.value)};")
        elif isinstance(item, IfStmt):
            self._emit_if(item)
        elif isinstance(item, WhileStmt):
            self._emit_while(item)
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
            self._emit_print(item.value)
        elif isinstance(item, ExprStmt):
            self._writeline(f"{self._expr(item.expr)};")
        elif isinstance(item, Block):
            self._emit_block_stmt(item)
        elif isinstance(item, FunctionDecl):
            return
        else:
            raise CodegenError(f"Unhandled item: {type(item).__name__}")

    def _emit_statements(self, statements: List[Stmt]) -> None:
        for stmt in statements:
            self._emit_item(stmt)

    def _emit_block_stmt(self, block: Block) -> None:
        self._writeline("{")
        self.indent += 1
        self._emit_statements(block.statements)
        self.indent -= 1
        self._writeline("}")

    def _emit_if(self, stmt: IfStmt) -> None:
        self._writeline(f"if ({self._expr(stmt.condition)}) {{")
        self.indent += 1
        self._emit_statements(stmt.then_block.statements)
        self.indent -= 1
        if stmt.else_block is None:
            self._writeline("}")
        else:
            self._writeline("} else {")
            self.indent += 1
            self._emit_statements(stmt.else_block.statements)
            self.indent -= 1
            self._writeline("}")

    def _emit_while(self, stmt: WhileStmt) -> None:
        self._writeline(f"while ({self._expr(stmt.condition)}) {{")
        self.indent += 1
        self._emit_statements(stmt.body.statements)
        self.indent -= 1
        self._writeline("}")

    def _emit_switch(self, stmt: SwitchStmt) -> None:
        self._writeline(f"switch ({self._expr(stmt.expr)}) {{")
        self.indent += 1
        for case in stmt.cases:
            self._writeline(f"case {self._expr(case.value)}:")
            self.indent += 1
            self._emit_statements(case.body.statements)
            self.indent -= 1
        if stmt.default is not None:
            self._writeline("default:")
            self.indent += 1
            self._emit_statements(stmt.default.statements)
            self.indent -= 1
        self.indent -= 1
        self._writeline("}")

    def _emit_for(self, stmt: ForStmt) -> None:
        init_str = ""
        if stmt.init is not None:
            if isinstance(stmt.init, VarDecl):
                init_str = f"{self._c_type(stmt.init.type.name)} {stmt.init.name} = {self._expr(stmt.init.value)}"
            elif isinstance(stmt.init, Assign):
                init_str = f"{stmt.init.name} = {self._expr(stmt.init.value)}"
            elif isinstance(stmt.init, ExprStmt):
                init_str = self._expr(stmt.init.expr)
        cond_str = self._expr(stmt.condition) if stmt.condition else ""
        inc_str = ""
        if stmt.increment is not None:
            if isinstance(stmt.increment, Assign):
                inc_str = f"{stmt.increment.name} = {self._expr(stmt.increment.value)}"
            elif isinstance(stmt.increment, ExprStmt):
                inc_str = self._expr(stmt.increment.expr)
        self._writeline(f"for ({init_str}; {cond_str}; {inc_str}) {{")
        self.indent += 1
        self._emit_statements(stmt.body.statements)
        self.indent -= 1
        self._writeline("}")
    def _emit_global_var(self, decl: VarDecl) -> None:
        ctype = self._c_type(decl.type.name)
        qualifier = "const " if not decl.mutable and not ctype.startswith("const ") else ""
        self._writeline(f"{qualifier}{ctype} {decl.name} = {self._expr(decl.value)};")

    def _emit_vardecl(self, decl: VarDecl) -> None:
        ctype = self._c_type(decl.type.name)
        qualifier = "const " if not decl.mutable and not ctype.startswith("const ") else ""
        self._writeline(f"{qualifier}{ctype} {decl.name} = {self._expr(decl.value)};")

    def _emit_print(self, expr: Expr) -> None:
        t = self._type_of(expr)
        text = self._expr(expr)
        if t.name == "int":
            self._writeline(f"selenium_print_int({text});")
        elif t.name == "float":
            self._writeline(f"selenium_print_float({text});")
        elif t.name == "bool":
            self._writeline(f"selenium_print_bool({text});")
        elif t.name == "char":
            self._writeline(f"selenium_print_char({text});")
        elif t.name == "string":
            self._writeline(f"selenium_print_string({text});")
        else:
            raise CodegenError(f"Cannot print type {t.name}")

    def _expr(self, expr: Expr) -> str:
        if isinstance(expr, Literal):
            if expr.kind == "string":
                return self._escape_string(expr.value)
            if expr.kind == "char":
                return self._escape_char(expr.value)
            if expr.kind == "bool":
                return "true" if expr.value else "false"
            return str(expr.value)
        if isinstance(expr, VarRef):
            return expr.name
        if isinstance(expr, Cast):
            target = self._c_type(expr.target_type.name)
            return f"(({target})({self._expr(expr.expr)}))"
        if isinstance(expr, Call):
            args = ", ".join(self._expr(arg) for arg in expr.args)
            return f"{expr.callee}({args})"
        if isinstance(expr, Unary):
            return f"({expr.op}{self._expr(expr.expr)})"
        if isinstance(expr, Ternary):
            cond = self._expr(expr.condition)
            then = self._expr(expr.then_expr)
            else_ = self._expr(expr.else_expr)
            return f"({cond} ? {then} : {else_})"
        if isinstance(expr, Binary):
            return f"({self._expr(expr.left)} {expr.op} {self._expr(expr.right)})"
        raise CodegenError(f"Unhandled expr: {type(expr).__name__}")

    def _type_of(self, expr: Expr) -> TypeInfo:
        try:
            return self.ctx.expr_types[id(expr)]
        except KeyError as exc:
            raise CodegenError(f"Missing inferred type for {type(expr).__name__}") from exc

    def _c_type(self, name: str) -> str:
        mapping = {
            "int": "int",
            "float": "double",
            "bool": "_Bool",
            "char": "char",
            "string": "const char *",
            "void": "void",
        }
        if name not in mapping:
            raise CodegenError(f"Unsupported C type: {name}")
        return mapping[name]

    def _escape_string(self, value: str) -> str:
        value = (
            value.replace("\\", "\\\\")
            .replace('"', '\\"')
            .replace("\n", "\\n")
            .replace("\t", "\\t")
            .replace("\r", "\\r")
        )
        return f'"{value}"'

    def _escape_char(self, value: str) -> str:
        mapping = {"\\": "\\\\", "'": "\\'", "\n": "\\n", "\t": "\\t", "\r": "\\r"}
        return f"'{mapping.get(value, value)}'"

    def _writeline(self, text: str) -> None:
        self.lines.append("    " * self.indent + text)
