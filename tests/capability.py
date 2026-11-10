"""Shared helpers for gating pytest suites by the capability matrix.

Tests for language features whose row is not yet implemented in the C++
port are skipped with the matrix status, keeping CI enforcement aligned
with the documented feature set (mirroring the differential runner).
"""

import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
_MATRIX = json.loads((ROOT / "capabilities.json").read_text(encoding="utf-8"))
_CELLS = {row["id"]: row["cells"]["selenium"] for row in _MATRIX["rows"]}


def cell(row_id: str) -> dict:
    return _CELLS[row_id]


def status(row_id: str) -> str:
    return _CELLS[row_id]["status"]
