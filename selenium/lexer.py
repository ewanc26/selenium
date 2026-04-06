from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List


class LexError(Exception):
    pass


@dataclass(slots=True)
class Token:
    kind: str
    value: Any
    line: int
    col: int


KEYWORDS = {
    "wax": "WAX",
    "seal": "SEAL",
    "ritual": "RITUAL",
    "eclipse": "ECLIPSE",
    "shadow": "SHADOW",
    "tide": "TIDE",
    "orbit": "ORBIT",
    "whisper": "WHISPER",
    "return": "RETURN",
    "cast": "CAST",
    "true": "BOOL",
    "false": "BOOL",
    "int": "TYPE",
    "float": "TYPE",
    "bool": "TYPE",
    "char": "TYPE",
    "string": "TYPE",
    "void": "TYPE",
}


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.length = len(source)
        self.i = 0
        self.line = 1
        self.col = 1

    def tokenize(self) -> List[Token]:
        tokens: List[Token] = []
        while not self._eof():
            ch = self._peek()
            if ch in " \t\r":
                self._advance()
                continue
            if ch == "\n":
                self._advance_line()
                continue
            if ch == "/" and self._peek(1) == "/":
                self._skip_line_comment()
                continue
            if ch == "/" and self._peek(1) == "*":
                self._skip_block_comment()
                continue

            start_line, start_col = self.line, self.col

            two = ch + self._peek(1)
            if two in {"->", "<=", ">=", "==", "!=", "&&", "||"}:
                tokens.append(Token(two, two, start_line, start_col))
                self._advance()
                self._advance()
                continue

            if ch.isalpha() or ch == "_":
                tokens.append(self._identifier())
                continue
            if ch.isdigit():
                tokens.append(self._number())
                continue
            if ch == '"':
                tokens.append(self._string())
                continue
            if ch == "'":
                tokens.append(self._char())
                continue

            if ch in {";", ",", "(", ")", "{", "}", "+", "-", "*", "/", "%", "=", "<", ">", "!", ":"}:
                tokens.append(Token(ch, ch, start_line, start_col))
                self._advance()
                continue

            raise LexError(f"Unexpected character {ch!r} at {start_line}:{start_col}")

        tokens.append(Token("EOF", None, self.line, self.col))
        return tokens

    def _eof(self) -> bool:
        return self.i >= self.length

    def _peek(self, offset: int = 0) -> str:
        pos = self.i + offset
        if pos >= self.length:
            return "\0"
        return self.source[pos]

    def _advance(self) -> str:
        ch = self.source[self.i]
        self.i += 1
        self.col += 1
        return ch

    def _advance_line(self) -> None:
        self.i += 1
        self.line += 1
        self.col = 1

    def _skip_line_comment(self) -> None:
        while not self._eof() and self._peek() != "\n":
            self._advance()
        if not self._eof() and self._peek() == "\n":
            self._advance_line()

    def _skip_block_comment(self) -> None:
        self._advance()
        self._advance()
        while not self._eof():
            if self._peek() == "*" and self._peek(1) == "/":
                self._advance()
                self._advance()
                return
            if self._peek() == "\n":
                self._advance_line()
            else:
                self._advance()
        raise LexError("Unterminated block comment")

    def _identifier(self) -> Token:
        start_line, start_col = self.line, self.col
        buf = []
        while not self._eof() and (self._peek().isalnum() or self._peek() == "_"):
            buf.append(self._advance())
        text = "".join(buf)
        kind = KEYWORDS.get(text, "IDENT")
        if kind == "BOOL":
            value = text == "true"
        else:
            value = text
        return Token(kind, value, start_line, start_col)

    def _number(self) -> Token:
        start_line, start_col = self.line, self.col
        buf = []
        has_dot = False
        while not self._eof():
            ch = self._peek()
            if ch == "." and not has_dot and self._peek(1).isdigit():
                has_dot = True
                buf.append(self._advance())
                continue
            if ch.isdigit():
                buf.append(self._advance())
                continue
            break
        text = "".join(buf)
        if has_dot:
            return Token("FLOAT", float(text), start_line, start_col)
        return Token("INT", int(text), start_line, start_col)

    def _string(self) -> Token:
        start_line, start_col = self.line, self.col
        self._advance()
        buf = []
        while not self._eof():
            ch = self._peek()
            if ch == '"':
                self._advance()
                return Token("STRING", "".join(buf), start_line, start_col)
            if ch == "\\":
                self._advance()
                if self._eof():
                    break
                esc = self._advance()
                mapping = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}
                buf.append(mapping.get(esc, esc))
                continue
            if ch == "\n":
                raise LexError(f"Unterminated string literal at {start_line}:{start_col}")
            buf.append(self._advance())
        raise LexError(f"Unterminated string literal at {start_line}:{start_col}")

    def _char(self) -> Token:
        start_line, start_col = self.line, self.col
        self._advance()
        if self._eof():
            raise LexError(f"Unterminated char literal at {start_line}:{start_col}")
        if self._peek() == "\\":
            self._advance()
            if self._eof():
                raise LexError(f"Unterminated char literal at {start_line}:{start_col}")
            esc = self._advance()
            mapping = {"n": "\n", "t": "\t", "r": "\r", "'": "'", '"': '"', "\\": "\\"}
            value = mapping.get(esc, esc)
        else:
            value = self._advance()
        if self._eof() or self._peek() != "'":
            raise LexError(f"Unterminated char literal at {start_line}:{start_col}")
        self._advance()
        return Token("CHAR", value, start_line, start_col)
