from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Union


@dataclass(slots=True)
class TypeRef:
    name: str


@dataclass(slots=True)
class Param:
    type: TypeRef
    name: str


@dataclass(slots=True)
class Program:
    items: List[object]


@dataclass(slots=True)
class Block:
    statements: List[object]


@dataclass(slots=True)
class VarDecl:
    mutable: bool
    type: TypeRef
    name: str
    value: "Expr"


@dataclass(slots=True)
class FunctionDecl:
    name: str
    params: List[Param]
    return_type: TypeRef
    body: Block


@dataclass(slots=True)
class Assign:
    name: str
    value: "Expr"


@dataclass(slots=True)
class IfStmt:
    condition: "Expr"
    then_block: Block
    else_block: Optional[Block]


@dataclass(slots=True)
class WhileStmt:
    condition: "Expr"
    body: Block


@dataclass(slots=True)
class ForStmt:
    init: Optional["Stmt"]
    condition: "Expr"
    increment: Optional["Stmt"]
    body: Block


@dataclass(slots=True)
class ReturnStmt:
    value: Optional["Expr"]


@dataclass(slots=True)
class BreakStmt:
    pass


@dataclass(slots=True)
class ContinueStmt:
    pass


@dataclass(slots=True)
class PrintStmt:
    value: "Expr"


@dataclass(slots=True)
class ExprStmt:
    expr: "Expr"


@dataclass(slots=True)
class Literal:
    value: object
    kind: str


@dataclass(slots=True)
class VarRef:
    name: str


@dataclass(slots=True)
class Unary:
    op: str
    expr: "Expr"


@dataclass(slots=True)
class Binary:
    left: "Expr"
    op: str
    right: "Expr"


@dataclass(slots=True)
class Call:
    callee: str
    args: List["Expr"]


@dataclass(slots=True)
class Cast:
    target_type: TypeRef
    expr: "Expr"


Expr = Union[Literal, VarRef, Unary, Binary, Call, Cast]
Stmt = Union[VarDecl, Assign, IfStmt, WhileStmt, ForStmt, ReturnStmt, BreakStmt, ContinueStmt, PrintStmt, ExprStmt, Block]
TopLevel = Union[VarDecl, FunctionDecl, Assign, IfStmt, WhileStmt, ForStmt, ReturnStmt, BreakStmt, ContinueStmt, PrintStmt, ExprStmt, Block]
