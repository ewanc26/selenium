from __future__ import annotations

from typing import List, Optional

from .ast import (
    Assign,
    Binary,
    Block,
    BreakStmt,
    Call,
    Cast,
    ContinueStmt,
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
from .lexer import Lexer, Token, LexError


class ParseError(Exception):
    pass


TYPE_TOKENS = {"TYPE"}


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.i = 0

    @classmethod
    def from_source(cls, source: str) -> "Parser":
        return cls(Lexer(source).tokenize())

    def parse(self) -> Program:
        items: List[TopLevel] = []
        while not self._check("EOF"):
            items.append(self._declaration_or_stmt())
        return Program(items)

    def _declaration_or_stmt(self) -> TopLevel:
        if self._match("RITUAL"):
            return self._function_decl()
        if self._match("WAX"):
            return self._var_decl(mutable=True)
        if self._match("SEAL"):
            return self._var_decl(mutable=False)
        return self._statement()

    def _function_decl(self) -> FunctionDecl:
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
        return_type = self._type_ref()
        body = self._block()
        self._consume(";", "Expected ';' after function body")
        return FunctionDecl(name, params, return_type, body)

    def _for_init(self) -> Optional[Stmt]:
        if self._match("WAX"):
            return self._var_decl_no_semi(mutable=True)
        if self._match("SEAL"):
            return self._var_decl_no_semi(mutable=False)
        if not self._check(";"):
            return self._statement()
        return None

    def _var_decl(self, mutable: bool) -> VarDecl:
        var_type = self._type_ref()
        name = self._consume("IDENT", "Expected variable name").value
        self._consume("=", "Expected '=' in declaration")
        value = self._expression()
        self._consume(";", "Expected ';' after declaration")
        return VarDecl(mutable, var_type, name, value)

    def _var_decl_no_semi(self, mutable: bool) -> VarDecl:
        var_type = self._type_ref()
        name = self._consume("IDENT", "Expected variable name").value
        self._consume("=", "Expected '=' in declaration")
        value = self._expression()
        return VarDecl(mutable, var_type, name, value)

    def _statement(self) -> Stmt:
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

        if self._match("TIDE"):
            self._consume("(", "Expected '(' after tide")
            cond = self._expression()
            self._consume(")", "Expected ')' after condition")
            body = self._block()
            self._consume(";", "Expected ';' after while statement")
            return WhileStmt(cond, body)

        if self._match("ORBIT"):
            self._consume("(", "Expected '(' after orbit")
            init = self._for_init()
            self._consume(";", "Expected ';' after init")
            cond = self._expression()
            self._consume(";", "Expected ';' after condition")
            increment = None
            if not self._check(")"):
                if self._check("IDENT") and self._check_next("="):
                    name = self._advance().value
                    self._advance()  # =
                    value = self._expression()
                    increment = Assign(name, value)
                else:
                    increment = ExprStmt(self._expression())
            self._consume(")", "Expected ')' after increment")
            body = self._block()
            self._consume(";", "Expected ';' after for statement")
            return ForStmt(init, cond, increment, body)

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

        if self._check("IDENT") and self._check_next("="):
            name = self._advance().value
            self._advance()  # =
            value = self._expression()
            self._consume(";", "Expected ';' after assignment")
            return Assign(name, value)

        if self._check("{"):
            block = self._block()
            self._consume(";", "Expected ';' after block")
            return block

        expr = self._expression()
        self._consume(";", "Expected ';' after expression")
        return ExprStmt(expr)

    def _block(self) -> Block:
        self._consume("{", "Expected '{' to start block")
        statements: List[Stmt] = []
        while not self._check("}"):
            statements.append(self._declaration_or_stmt())
        self._consume("}", "Expected '}' after block")
        return Block(statements)

    def _type_ref(self) -> TypeRef:
        tok = self._consume("TYPE", "Expected type name")
        return TypeRef(tok.value)

    def _expression(self) -> Expr:
        return self._or()

    def _or(self) -> Expr:
        expr = self._and()
        while self._match("||"):
            op = "||"
            right = self._and()
            expr = Binary(expr, op, right)
        return expr

    def _and(self) -> Expr:
        expr = self._equality()
        while self._match("&&"):
            op = "&&"
            right = self._equality()
            expr = Binary(expr, op, right)
        return expr

    def _equality(self) -> Expr:
        expr = self._comparison()
        while self._match("==", "!="):
            op = self._previous().kind
            right = self._comparison()
            expr = Binary(expr, op, right)
        return expr

    def _comparison(self) -> Expr:
        expr = self._term()
        while self._match("<", "<=", ">", ">="):
            op = self._previous().kind
            right = self._term()
            expr = Binary(expr, op, right)
        return expr

    def _term(self) -> Expr:
        expr = self._factor()
        while self._match("+", "-"):
            op = self._previous().kind
            right = self._factor()
            expr = Binary(expr, op, right)
        return expr

    def _factor(self) -> Expr:
        expr = self._unary()
        while self._match("*", "/", "%"):
            op = self._previous().kind
            right = self._unary()
            expr = Binary(expr, op, right)
        return expr

    def _unary(self) -> Expr:
        if self._match("!", "-", "++", "--"):
            op = self._previous().kind
            expr = self._unary()
            return Unary(op, expr)
        return self._call()

    def _call(self) -> Expr:
        expr = self._primary()
        while self._match("("):
            if not isinstance(expr, VarRef):
                raise self._error(self._previous(), "Only named functions can be called")
            args: List[Expr] = []
            if not self._check(")"):
                while True:
                    args.append(self._expression())
                    if not self._match(","):
                        break
            self._consume(")", "Expected ')' after arguments")
            expr = Call(expr.name, args)
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
        if self._match("IDENT"):
            return VarRef(self._previous().value)
        if self._match("("):
            expr = self._expression()
            self._consume(")", "Expected ')'")
            return expr
        if self._match("CAST"):
            self._consume("(", "Expected '(' after cast")
            target = self._type_ref()
            self._consume(",", "Expected ',' in cast")
            expr = self._expression()
            self._consume(")", "Expected ')' after cast")
            return Cast(target, expr)
        raise self._error(self._peek(), "Expected expression")

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
