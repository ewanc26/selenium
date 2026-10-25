"""AST node types for the Selenium language.

Every construct in the language maps to a dataclass here.
The parse phase produces these; the semantic analyser and code
generator consume them by isinstance-dispatch."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Union


@dataclass(slots=True)
class FnTypeRef:
    """Function type written as ``ritual(params) -> ret``."""

    params: List["TypeRef"]
    ret: "TypeRef"


@dataclass(slots=True)
class TypeRef:
    """A type as written in source.

    ``name`` is a builtin type, a struct/union/enum tag or an alias. ``pointer``
    counts ``*`` suffixes and ``dims`` lists ``[N]`` suffixes outermost first.
    ``fn`` is set for function types, whose name is ``fn``."""

    name: str
    pointer: int = 0
    dims: List[int] = field(default_factory=list)
    fn: Optional[FnTypeRef] = None


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
    qualifiers: Tuple[str, ...] = ()


@dataclass(slots=True)
class FunctionDecl:
    name: str
    params: List[Param]
    return_type: TypeRef
    body: Optional[Block]
    source: Optional[str] = None


@dataclass(slots=True)
class StructDecl:
    name: str
    fields: List[Param]


@dataclass(slots=True)
class UnionDecl:
    name: str
    fields: List[Param]


@dataclass(slots=True)
class EnumDecl:
    name: str
    enumerators: List[Tuple[str, Optional[int]]]


@dataclass(slots=True)
class AliasDecl:
    name: str
    target: TypeRef


@dataclass(slots=True)
class Assign:
    target: "Expr"
    value: "Expr"


@dataclass(slots=True)
class CompoundAssign:
    target: "Expr"
    op: str
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
class DoWhileStmt:
    body: Block
    condition: "Expr"


@dataclass(slots=True)
class GotoStmt:
    label: str


@dataclass(slots=True)
class Label:
    name: str
    body: Block


@dataclass(slots=True)
class SizeofExpr:
    expr: Optional["Expr"] = None
    target_type: Optional[TypeRef] = None


@dataclass(slots=True)
class Case:
    value: "Expr"
    body: Block


@dataclass(slots=True)
class SwitchStmt:
    expr: "Expr"
    cases: List[Case]
    default: Optional[Block]


@dataclass(slots=True)
class ForStmt:
    init: Optional[object]
    condition: "Expr"
    increment: Optional[object]
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
class Postfix:
    op: str
    expr: "Expr"


@dataclass(slots=True)
class Ternary:
    condition: "Expr"
    then_expr: "Expr"
    else_expr: "Expr"


@dataclass(slots=True)
class Binary:
    left: "Expr"
    op: str
    right: "Expr"


@dataclass(slots=True)
class Call:
    callee: "Expr"
    args: List["Expr"]


@dataclass(slots=True)
class ArrayLit:
    elements: List["Expr"]


@dataclass(slots=True)
class BraceLit:
    elements: List["Expr"]


@dataclass(slots=True)
class Index:
    target: "Expr"
    index: "Expr"


@dataclass(slots=True)
class Member:
    target: "Expr"
    name: str
    arrow: bool


@dataclass(slots=True)
class Cast:
    target_type: TypeRef
    expr: "Expr"


Expr = Union[
    Literal,
    VarRef,
    Unary,
    Postfix,
    Ternary,
    Binary,
    Call,
    Cast,
    SizeofExpr,
    ArrayLit,
    BraceLit,
    Index,
    Member,
]
Stmt = Union[
    VarDecl,
    Assign,
    CompoundAssign,
    IfStmt,
    WhileStmt,
    DoWhileStmt,
    GotoStmt,
    Label,
    SwitchStmt,
    ForStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    PrintStmt,
    ExprStmt,
    Block,
]
TopLevel = Union[
    VarDecl,
    FunctionDecl,
    StructDecl,
    UnionDecl,
    EnumDecl,
    AliasDecl,
    Stmt,
]
