"""Recursive-descent parser for Selenium.

Tokenises source via the lexer, then walks a precedence-climbing expression
parser (C operator precedence) inside a statement-first top-down structure.
Produces a Program AST."""

from __future__ import annotations

from typing import List, Optional, Set

from .ast import (
    AliasDecl,
    ArrayLit,
    Assign,
    Binary,
    Block,
    BraceLit,
    BreakStmt,
    Call,
    Case,
    Cast,
    CompoundAssign,
    ContinueStmt,
    DoWhileStmt,
    EnumDecl,
    Expr,
    ExprStmt,
    FnTypeRef,
    ForStmt,
    FunctionDecl,
    GotoStmt,
    IfStmt,
    Index,
    Label,
    Literal,
    Member,
    Param,
    Postfix,
    PrintStmt,
    Program,
    ReturnStmt,
    SizeofExpr,
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
from .lexer import Lexer, LexError, Token


class ParseError(Exception):
    """Raised on syntax errors with line/col context and expected token details."""


ASSIGN_OPS = {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}


class Parser:
    """Recursive-descent parser over a token stream.

    Expression parsing follows C operator precedence. Type names introduced by
    frame, fusion, phase and alias declarations are tracked so that they can be
    used as types after they are declared."""

    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.i = 0
        self.type_names: Set[str] = set()

    @classmethod
    def from_source(cls, source: str) -> "Parser":
        """Convenience: lex source, return parser ready to go."""
        return cls(Lexer(source).tokenize())

    def parse(self) -> Program:
        """Parse the full token stream into a Program AST."""

        items: List[object] = []
        while not self._check("EOF"):
            items.append(self._declaration_or_stmt(top=True))
        return Program(items)

    # ── Top-level dispatch ────────────────────────────────────────────

    def _declaration_or_stmt(self, top: bool = False):
        if self._check("RITUAL") or self._check("SUMMON"):
            if not top:
                raise self._error(
                    self._peek(), "Function declarations are only allowed at top level"
                )
            if self._match("RITUAL"):
                return self._function_decl()
            self._advance()  # SUMMON
            return self._extern_decl()
        if self._match("WAX"):
            return self._var_decl(mutable=True)
        if self._match("SEAL"):
            return self._var_decl(mutable=False)
        if (
            self._check("FRAME")
            or self._check("FUSION")
            or self._check("PHASE")
            or self._check("ALIAS")
        ):
            if not top:
                raise self._error(
                    self._peek(), "Type declarations are only allowed at top level"
                )
            return self._type_decl()
        return self._statement()

    def _type_decl(self):
        kind = self._advance().kind
        if kind == "ALIAS":
            name = self._consume("IDENT", "Expected alias name").value
            self._consume("=", "Expected '=' in alias declaration")
            target = self._type_ref()
            self._consume(";", "Expected ';' after alias declaration")
            self.type_names.add(name)
            return AliasDecl(name, target)
        name = self._consume("IDENT", "Expected type name").value
        self.type_names.add(name)
        if kind == "PHASE":
            self._consume("{", "Expected '{' after phase name")
            enumerators = []
            while not self._check("}"):
                ename = self._consume("IDENT", "Expected enumerator name").value
                value = None
                if self._match("="):
                    negative = self._match("-")
                    value = self._consume(
                        "INT", "Expected integer enumerator value"
                    ).value
                    if negative:
                        value = -value
                enumerators.append((ename, value))
                if not self._match(","):
                    break
            self._consume("}", "Expected '}' after enumerators")
            self._consume(";", "Expected ';' after phase declaration")
            return EnumDecl(name, enumerators)
        self._consume("{", "Expected '{' after type name")
        fields: List[Param] = []
        while not self._check("}"):
            ftype = self._type_ref()
            fname = self._consume("IDENT", "Expected field name").value
            self._consume(";", "Expected ';' after field")
            fields.append(Param(ftype, fname))
        self._consume("}", "Expected '}' after fields")
        self._consume(";", "Expected ';' after type declaration")
        if kind == "FRAME":
            return StructDecl(name, fields)
        return UnionDecl(name, fields)

    def _function_decl(self) -> FunctionDecl:
        name, params, return_type = self._signature()
        body = self._block()
        self._consume(";", "Expected ';' after function body")
        return FunctionDecl(name, params, return_type, body)

    def _extern_decl(self) -> FunctionDecl:
        self._consume("RITUAL", "Expected 'ritual' after 'summon'")
        name, params, return_type = self._signature()
        source = None
        if self._match("FROM"):
            source = self._consume(
                "STRING", "Expected C source path after 'from'"
            ).value
        self._consume(";", "Expected ';' after summoned function signature")
        return FunctionDecl(name, params, return_type, None, source)

    def _signature(self):
        name = self._consume("IDENT", "Expected function name").value
        self._consume("(", "Expected '(' after function name")
        params: List[Param] = []
        if not self._check(")"):
            while True:
                ptype = self._type_ref()
                pname = self._consume("IDENT", "Expected parameter name").value
                params.append(Param(ptype, pname))
                if not self._match(","):
                    break
        self._consume(")", "Expected ')' after parameters")
        self._consume("->", "Expected '->' before return type")
        return name, params, self._type_ref()

    def _for_init(self):
        if self._match("WAX"):
            return self._var_decl_no_semi(mutable=True)
        if self._match("SEAL"):
            return self._var_decl_no_semi(mutable=False)
        if not self._check(";"):
            return self._simple_statement()
        return None

    def _qualifiers(self) -> tuple:
        quals = []
        while self._check("PINNED") or self._check("ERRATIC"):
            quals.append(self._advance().value)
        return tuple(quals)

    def _var_decl(self, mutable: bool) -> VarDecl:
        quals = self._qualifiers()
        var_type = self._type_ref()
        name = self._consume("IDENT", "Expected variable name").value
        self._consume("=", "Expected '=' in declaration")
        value = self._initializer()
        self._consume(";", "Expected ';' after declaration")
        return VarDecl(mutable, var_type, name, value, quals)

    def _var_decl_no_semi(self, mutable: bool) -> VarDecl:
        quals = self._qualifiers()
        var_type = self._type_ref()
        name = self._consume("IDENT", "Expected variable name").value
        self._consume("=", "Expected '=' in declaration")
        value = self._initializer()
        return VarDecl(mutable, var_type, name, value, quals)

    def _initializer(self) -> Expr:
        return self._expression()

    # ── Statement parsing ───────────────────────────────────────────

    def _statement(self):
        if self._match("ECLIPSE"):
            self._consume("(", "Expected '(' after eclipse")
            cond = self._expression()
            self._consume(")", "Expected ')' after condition")
            then_block = self._block()
            else_block = None
            if self._match("SHADOW"):
                else_block = self._block()
            self._consume(";", "Expected ';' after if statement")
            return IfStmt(cond, then_block, else_block)

        if self._match("UNDERTOW"):
            body = self._block()
            self._consume("TIDE", "Expected 'tide' after undertow body")
            self._consume("(", "Expected '(' after tide")
            cond = self._expression()
            self._consume(")", "Expected ')' after condition")
            self._consume(";", "Expected ';' after undertow statement")
            return DoWhileStmt(body, cond)

        if self._match("TIDE"):
            self._consume("(", "Expected '(' after tide")
            cond = self._expression()
            self._consume(")", "Expected ')' after condition")
            body = self._block()
            self._consume(";", "Expected ';' after while statement")
            return WhileStmt(cond, body)

        if self._match("DRIFT"):
            label = self._consume("IDENT", "Expected label name after drift").value
            self._consume(";", "Expected ';' after drift")
            return GotoStmt(label)

        if self._check("IDENT") and self._check_next(":"):
            label_name = self._advance().value
            self._consume(":", "Expected ':' after label name")
            stmt = self._statement()
            return Label(
                label_name, Block([stmt]) if not isinstance(stmt, Block) else stmt
            )

        if self._match("ORBIT"):
            self._consume("(", "Expected '(' after orbit")
            init = self._for_init()
            self._consume(";", "Expected ';' after init")
            cond = self._expression()
            self._consume(";", "Expected ';' after condition")
            increment = None
            if not self._check(")"):
                increment = self._simple_statement()
            self._consume(")", "Expected ')' after increment")
            body = self._block()
            self._consume(";", "Expected ';' after for statement")
            return ForStmt(init, cond, increment, body)

        if self._match("SWITCH"):
            self._consume("(", "Expected '(' after switch")
            expr = self._expression()
            self._consume(")", "Expected ')' after expression")
            self._consume("{", "Expected '{' after switch")
            cases: List[Case] = []
            default = None
            while not self._check("}"):
                if self._match("CASE"):
                    value = self._expression()
                    self._consume(":", "Expected ':' after case value")
                    body = self._block()
                    self._consume(";", "Expected ';' after case body")
                    cases.append(Case(value, body))
                elif self._match("DEFAULT"):
                    self._consume(":", "Expected ':' after default")
                    default = self._block()
                    self._consume(";", "Expected ';' after default body")
                else:
                    raise ParseError("Expected case or default in switch")
            self._consume("}", "Expected '}' after switch")
            self._consume(";", "Expected ';' after switch statement")
            return SwitchStmt(expr, cases, default)

        if self._match("RETURN"):
            if self._check(";"):
                self._advance()
                return ReturnStmt(None)
            value = self._expression()
            self._consume(";", "Expected ';' after return")
            return ReturnStmt(value)

        if self._match("BREAK"):
            self._consume(";", "Expected ';' after break")
            return BreakStmt()

        if self._match("CONTINUE"):
            self._consume(";", "Expected ';' after continue")
            return ContinueStmt()

        if self._match("WHISPER"):
            value = self._expression()
            self._consume(";", "Expected ';' after whisper")
            return PrintStmt(value)

        if self._check("{"):
            block = self._block()
            self._consume(";", "Expected ';' after block")
            return block

        stmt = self._simple_statement()
        self._consume(";", "Expected ';' after statement")
        return stmt

    def _simple_statement(self):
        """An expression, assignment or compound assignment, without the ';'."""
        expr = self._expression()
        if self._match("="):
            value = self._expression()
            return Assign(expr, value)
        for op in sorted(ASSIGN_OPS):
            if self._match(op):
                value = self._expression()
                return CompoundAssign(expr, op[:-1], value)
        return ExprStmt(expr)

    def _block(self) -> Block:
        self._consume("{", "Expected '{' to start block")
        statements: List[object] = []
        while not self._check("}"):
            statements.append(self._declaration_or_stmt())
        self._consume("}", "Expected '}' after block")
        return Block(statements)

    def _type_ref(self) -> TypeRef:
        if self._match("RITUAL"):
            self._consume("(", "Expected '(' in function type")
            params: List[TypeRef] = []
            if not self._check(")"):
                while True:
                    params.append(self._type_ref())
                    if not self._match(","):
                        break
            self._consume(")", "Expected ')' in function type")
            self._consume("->", "Expected '->' in function type")
            return TypeRef("fn", fn=FnTypeRef(params, self._type_ref()))
        if self._check("TYPE"):
            name = self._advance().value
        elif self._check("IDENT") and self._peek().value in self.type_names:
            name = self._advance().value
        else:
            raise self._error(self._peek(), "Expected type name")
        pointer = 0
        while self._match("*"):
            pointer += 1
        dims: List[int] = []
        while self._match("["):
            dims.append(self._consume("INT", "Expected array length").value)
            self._consume("]", "Expected ']' after array length")
        return TypeRef(name, pointer, dims)

    def _is_type_start(self) -> bool:
        if self._check("TYPE") or self._check("RITUAL"):
            return True
        return self._check("IDENT") and self._peek().value in self.type_names

    # ── Expression parsing (C operator precedence) ─────────────────

    def _expression(self) -> Expr:
        return self._ternary()

    def _ternary(self) -> Expr:
        expr = self._or()
        if self._match("?"):
            then_expr = self._expression()
            self._consume(":", "Expected ':' in ternary")
            else_expr = self._ternary()
            return Ternary(expr, then_expr, else_expr)
        return expr

    def _binary_level(self, operand, ops):
        def parse() -> Expr:
            expr = operand()
            while self._match(*ops):
                op = self._previous().kind
                expr = Binary(expr, op, operand())
            return expr

        return parse

    def _or(self) -> Expr:
        return self._binary_level(self._and, ("||",))()

    def _and(self) -> Expr:
        return self._binary_level(self._bitwise_or, ("&&",))()

    def _bitwise_or(self) -> Expr:
        return self._binary_level(self._bitwise_xor, ("|",))()

    def _bitwise_xor(self) -> Expr:
        return self._binary_level(self._bitwise_and, ("^",))()

    def _bitwise_and(self) -> Expr:
        return self._binary_level(self._equality, ("&",))()

    def _equality(self) -> Expr:
        return self._binary_level(self._comparison, ("==", "!="))()

    def _comparison(self) -> Expr:
        return self._binary_level(self._shift, ("<", "<=", ">", ">="))()

    def _shift(self) -> Expr:
        return self._binary_level(self._term, ("<<", ">>"))()

    def _term(self) -> Expr:
        return self._binary_level(self._factor, ("+", "-"))()

    def _factor(self) -> Expr:
        return self._binary_level(self._unary, ("*", "/", "%"))()

    def _unary(self) -> Expr:
        if self._match("!", "-", "~", "++", "--", "*", "&"):
            op = self._previous().kind
            return Unary(op, self._unary())
        if self._match("MEASURE"):
            self._consume("(", "Expected '(' after measure")
            if self._is_type_start():
                tref = self._type_ref()
                self._consume(")", "Expected ')' in measure")
                return SizeofExpr(target_type=tref)
            expr = self._expression()
            self._consume(")", "Expected ')' in measure")
            return SizeofExpr(expr=expr)
        return self._postfix()

    def _postfix(self) -> Expr:
        expr = self._primary()
        while True:
            if self._match("["):
                index = self._expression()
                self._consume("]", "Expected ']' after index")
                expr = Index(expr, index)
            elif self._match("("):
                args: List[Expr] = []
                if not self._check(")"):
                    while True:
                        args.append(self._expression())
                        if not self._match(","):
                            break
                self._consume(")", "Expected ')' after arguments")
                expr = Call(expr, args)
            elif self._match("."):
                name = self._consume("IDENT", "Expected member name after '.'").value
                expr = Member(expr, name, arrow=False)
            elif self._match("->"):
                name = self._consume("IDENT", "Expected member name after '->'").value
                expr = Member(expr, name, arrow=True)
            elif self._match("++", "--"):
                expr = Postfix(self._previous().kind, expr)
            else:
                return expr

    def _primary(self) -> Expr:
        if self._match("INT"):
            return Literal(self._previous().value, "int")
        if self._match("FLOAT"):
            return Literal(self._previous().value, "float")
        if self._match("STRING"):
            return Literal(self._previous().value, "string")
        if self._match("CHAR"):
            return Literal(self._previous().value, "char")
        if self._match("BOOL"):
            return Literal(self._previous().value, "bool")
        if self._match("NULLPTR"):
            return Literal(None, "nullptr")
        if self._match("IDENT"):
            return VarRef(self._previous().value)
        if self._match("("):
            expr = self._expression()
            while self._match(","):
                expr = Binary(expr, ",", self._expression())
            self._consume(")", "Expected ')'")
            return expr
        if self._match("["):
            return ArrayLit(self._list_items("]", "Expected ']' after array elements"))
        if self._match("{"):
            return BraceLit(
                self._list_items("}", "Expected '}' after initializer list")
            )
        if self._match("CAST"):
            self._consume("(", "Expected '(' after cast")
            target = self._type_ref()
            self._consume(",", "Expected ',' in cast")
            expr = self._expression()
            self._consume(")", "Expected ')' after cast")
            return Cast(target, expr)
        raise self._error(self._peek(), "Expected expression")

    def _list_items(self, closer: str, message: str) -> List[Expr]:
        elements: List[Expr] = []
        if not self._check(closer):
            while True:
                elements.append(self._expression())
                if not self._match(","):
                    break
        self._consume(closer, message)
        return elements

    # ── Parser primitives ───────────────────────────────────────────

    def _match(self, *kinds: str) -> bool:
        for kind in kinds:
            if self._check(kind):
                self._advance()
                return True
        return False

    def _consume(self, kind: str, message: str) -> Token:
        if self._check(kind):
            return self._advance()
        raise self._error(self._peek(), message)

    def _check(self, kind: str) -> bool:
        return self._peek().kind == kind

    def _check_next(self, kind: str) -> bool:
        if self.i + 1 >= len(self.tokens):
            return False
        return self.tokens[self.i + 1].kind == kind

    def _advance(self) -> Token:
        tok = self.tokens[self.i]
        if not self._check("EOF"):
            self.i += 1
        return tok

    def _peek(self) -> Token:
        return self.tokens[self.i]

    def _previous(self) -> Token:
        return self.tokens[self.i - 1]

    def _error(self, tok: Token, message: str) -> ParseError:
        return ParseError(f"{message} at {tok.line}:{tok.col}; got {tok.kind}")
