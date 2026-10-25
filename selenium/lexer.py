"""Tokeniser for the Selenium language.

Maps source text to a stream of tokens: keywords, literals, operators,
and delimiters. Every token carries line and column for error reporting."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List


class LexError(Exception):
    """Raised on invalid tokens, unterminated literals, or unexpected characters."""


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
    "summon": "SUMMON",
    "from": "FROM",
    "eclipse": "ECLIPSE",
    "shadow": "SHADOW",
    "tide": "TIDE",
    "undertow": "UNDERTOW",
    "orbit": "ORBIT",
    "switch": "SWITCH",
    "case": "CASE",
    "default": "DEFAULT",
    "break": "BREAK",
    "continue": "CONTINUE",
    "drift": "DRIFT",
    "whisper": "WHISPER",
    "return": "RETURN",
    "cast": "CAST",
    "measure": "MEASURE",
    "true": "BOOL",
    "false": "BOOL",
    "int": "TYPE",
    "float": "TYPE",
    "single": "TYPE",
    "ldouble": "TYPE",
    "bool": "TYPE",
    "schar": "TYPE",
    "uchar": "TYPE",
    "ushort": "TYPE",
    "uint": "TYPE",
    "ulong": "TYPE",
    "char": "TYPE",
    "string": "TYPE",
    "void": "TYPE",
    "alignas": "ALIGNAS",
    "alignof": "ALIGNOF",
    "auto": "AUTO",
    "const": "CONST",
    "constexpr": "CONSTEXPR",
    "do": "DO",
    "double": "DOUBLE",
    "else": "ELSE",
    "enum": "ENUM",
    "extern": "EXTERN",
    "for": "FOR",
    "goto": "GOTO",
    "if": "IF",
    "inline": "INLINE",
    "long": "TYPE",
    "nullptr": "NULLPTR",
    "register": "REGISTER",
    "restrict": "RESTRICT",
    "short": "TYPE",
    "signed": "SIGNED",
    "sizeof": "SIZEOF",
    "static": "STATIC",
    "static_assert": "STATIC_ASSERT",
    "struct": "STRUCT",
    "thread_local": "THREAD_LOCAL",
    "typedef": "TYPEDEF",
    "typeof": "TYPEOF",
    "typeof_unqual": "TYPEOF_UNQUAL",
    "union": "UNION",
    "unsigned": "UNSIGNED",
    "volatile": "VOLATILE",
    "while": "WHILE",
    "_Alignas": "ALIGNAS",
    "_Alignof": "ALIGNOF",
    "_Atomic": "ATOMIC",
    "_Bool": "_BOOL",
    "_Complex": "COMPLEX",
    "_Decimal128": "DECIMAL128",
    "_Decimal32": "DECIMAL32",
    "_Decimal64": "DECIMAL64",
    "_Generic": "GENERIC",
    "_Imaginary": "IMAGINARY",
    "_Noreturn": "NORETURN",
    "_Static_assert": "STATIC_ASSERT",
    "_Thread_local": "THREAD_LOCAL",
    "_BitInt": "BITINT",
    "asm": "ASM",
    "fortran": "FORTRAN",
    "frame": "FRAME",
    "fusion": "FUSION",
    "phase": "PHASE",
    "alias": "ALIAS",
    "pinned": "PINNED",
    "erratic": "ERRATIC",
}


class Lexer:
    """Scan source text left-to-right, producing a token list.

    Handles line comments (//), block comments (/* */), identifiers,
    numeric literals (int and float), string/char literals with escape
    sequences, and all operator symbols."""

    def __init__(self, source: str):
        self.source = source
        self.length = len(source)
        self.i = 0
        self.line = 1
        self.col = 1

    def tokenize(self) -> List[Token]:
        """Scan the entire source and return the token stream."""
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

            three = ch + self._peek(1) + self._peek(2)
            if three in {"<<=", ">>="}:
                tokens.append(Token(three, three, start_line, start_col))
                self._advance()
                self._advance()
                self._advance()
                continue

            two = ch + self._peek(1)
            if two in {"->", "<=", ">=", "==", "!=", "&&", "||"}:
                tokens.append(Token(two, two, start_line, start_col))
                self._advance()
                self._advance()
                continue
            if two in {"<<", ">>"}:
                tokens.append(Token(two, two, start_line, start_col))
                self._advance()
                self._advance()
                continue
            if two in {"+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="}:
                tokens.append(Token(two, two, start_line, start_col))
                self._advance()
                self._advance()
                continue
            if two in {"++", "--"}:
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

            if ch in {
                ";",
                ",",
                "(",
                ")",
                "{",
                "}",
                "[",
                "]",
                "+",
                "-",
                "*",
                "/",
                "%",
                "=",
                "<",
                ">",
                "!",
                ":",
                "?",
                "&",
                "|",
                "^",
                ".",
                "~",
            }:
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
        if self._peek() == "0" and self._peek(1) in {"x", "X", "b", "B"}:
            base = 16 if self._peek(1) in {"x", "X"} else 2
            digits = "0123456789abcdefABCDEF" if base == 16 else "01"
            self._advance()
            self._advance()
            buf = []
            while not self._eof() and self._peek() in digits:
                buf.append(self._advance())
            if not buf:
                raise LexError(f"Missing digits in literal at {start_line}:{start_col}")
            return Token("INT", int("".join(buf), base), start_line, start_col)
        buf = []
        is_float = False
        while not self._eof() and self._peek().isdigit():
            buf.append(self._advance())
        if self._peek() == "." and self._peek(1).isdigit():
            is_float = True
            buf.append(self._advance())
            while not self._eof() and self._peek().isdigit():
                buf.append(self._advance())
        if self._peek() in {"e", "E"} and (
            self._peek(1).isdigit()
            or (self._peek(1) in {"+", "-"} and self._peek(2).isdigit())
        ):
            is_float = True
            buf.append(self._advance())
            if self._peek() in {"+", "-"}:
                buf.append(self._advance())
            while not self._eof() and self._peek().isdigit():
                buf.append(self._advance())
        text = "".join(buf)
        if is_float:
            return Token("FLOAT", float(text), start_line, start_col)
        if len(text) > 1 and text.startswith("0"):
            if any(ch in "89" for ch in text):
                raise LexError(f"Invalid octal literal at {start_line}:{start_col}")
            return Token("INT", int(text, 8), start_line, start_col)
        return Token("INT", int(text), start_line, start_col)

    def _escape(self) -> str:
        """Read one escape sequence; the leading backslash is already consumed."""
        simple = {
            "n": "\n",
            "t": "\t",
            "r": "\r",
            "a": "\a",
            "b": "\b",
            "f": "\f",
            "v": "\v",
            "\\": "\\",
            "'": "'",
            '"': '"',
            "?": "?",
        }
        if self._peek() in "01234567":
            buf = []
            while len(buf) < 3 and not self._eof() and self._peek() in "01234567":
                buf.append(self._advance())
            return chr(int("".join(buf), 8))
        if self._peek() == "x":
            self._advance()
            buf = []
            while not self._eof() and self._peek() in "0123456789abcdefABCDEF":
                buf.append(self._advance())
            if not buf:
                raise LexError(
                    f"Missing hex digits in escape at {self.line}:{self.col}"
                )
            return chr(int("".join(buf), 16) & 0xFF)
        esc = self._advance()
        return simple.get(esc, esc)

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
                buf.append(self._escape())
                continue
            if ch == "\n":
                raise LexError(
                    f"Unterminated string literal at {start_line}:{start_col}"
                )
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
            value = self._escape()
        else:
            value = self._advance()
        if self._eof() or self._peek() != "'":
            raise LexError(f"Unterminated char literal at {start_line}:{start_col}")
        self._advance()
        return Token("CHAR", value, start_line, start_col)
