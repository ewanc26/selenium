#!/usr/bin/env python3
"""Differential tests: selenium programs versus equivalent C programs.

Usage: tests/differential/run_differential.py [--cc CC] [--strict]

Each NAME.sel has a NAME.c expressing the same operations. Both are run and
their stdout and exit status must match. The C side is compiled with
-std=c23 -Wall -Wextra -Werror -pedantic.

Cases map to rows of the shared capability matrix. By default only cases
whose row is 'implemented' for selenium are checked; the rest are reported
as SKIP with the matrix status, keeping the suite green for the real
feature set. --strict checks every case regardless of the matrix.
"""

import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
C_FLAGS = ["-std=c23", "-Wall", "-Wextra", "-Werror", "-pedantic"]

# differential case -> capability row
CASE_ROW = {
    "arithmetic": "expr.operators",
    "arrays": "types.fixed-arrays",
    "bitwise": "expr.operators",
    "break_continue": "control.break-continue",
    "casts": "expr.casts",
    "char_literals": "types.char-literals",
    "comma": "expr.comma",
    "compound_assign": "expr.compound-assign",
    "console": "io.console-output",
    "do_while": "control.do-while",
    "enums": "types.enums",
    "ffi_call": "ffi.call-c",
    "floating": "types.floating",
    "for_loop": "control.while-for",
    "func_pointers": "func.pointers",
    "func_recursion": "func.recursion",
    "functions": "func.params-returns",
    "global_addresses": "types.pointers",
    "goto": "control.goto",
    "if_else": "control.if-else",
    "incdec": "expr.incdec",
    "initializers": "types.initializers",
    "literals_escapes": "literals.escapes",
    "literals_numeric": "literals.numeric",
    "logical": "types.integer-boolean",
    "multidim_arrays": "types.multidim-arrays",
    "nullptr": "types.pointers",
    "pointers": "types.pointers",
    "program_calculator": None,  # composite; needs fixed-arrays
    "program_records": None,  # composite; needs structs
    "program_recursion": None,  # composite; needs fixed-arrays (recursion is covered by func_recursion)
    "sized_integers": "types.sized-integers",
    "sizeof": "types.sizeof",
    "storage_qualifiers": "types.storage-qualifiers",
    "strings": "types.strings",
    "structs": "types.structs",
    "switch": "control.switch",
    "ternary": "expr.ternary",
    "typedef": "types.typedef",
    "unions": "types.unions",
    "variables": "vars.assignment",
    "while_loop": "control.while-for",
}


def run(cmd, **kwargs):
    # Byte mode: some cases emit non-UTF-8 bytes (raw escapes), and the
    # comparison must be exact.
    return subprocess.run(cmd, capture_output=True, timeout=60, **kwargs)


def preview(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def matrix_lookup(source: pathlib.Path):
    row = CASE_ROW.get(source.stem)
    if row is None or row not in MATRIX_BY_ID:
        return None, None
    return row, MATRIX_BY_ID[row].get("status")


def check(source: pathlib.Path, cc: str, tmp: pathlib.Path) -> str:
    seleniumc = ROOT / "build" / "seleniumc"
    sl = run(
        [
            str(seleniumc),
            str(source),
            "--run",
            "--cc",
            cc,
        ],
        cwd=ROOT,
    )
    c_src = source.with_suffix(".c")
    c_bin = tmp / f"{source.stem}_ref"
    build = run([cc, *C_FLAGS, str(c_src), "-o", str(c_bin)])
    if build.returncode != 0:
        print(f"FAIL {source.stem}: C reference did not build\n{preview(build.stderr)}")
        return "fail"
    ref = run([str(c_bin)])
    if sl.stdout != ref.stdout or sl.returncode != ref.returncode:
        print(f"FAIL {source.stem}: stdout or exit status differs")
        print(
            f"  selenium: {sl.returncode} {preview(sl.stdout)!r} {preview(sl.stderr).strip()}"
        )
        print(f"  C:        {ref.returncode} {preview(ref.stdout)!r}")
        return "fail"
    print(f"PASS {source.stem}")
    return "pass"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default="cc")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="check every case, ignoring the capability matrix",
    )
    args = parser.parse_args()

    global MATRIX_BY_ID
    matrix_path = ROOT / "capabilities.json"
    if matrix_path.exists():
        matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
        MATRIX_BY_ID = {
            row["id"]: row["cells"].get("selenium", {}) for row in matrix["rows"]
        }
    else:
        MATRIX_BY_ID = {}

    sources = sorted(HERE.glob("*.sel"))
    if not sources:
        print("no differential cases found", file=sys.stderr)
        raise SystemExit(1)

    passed = failed = skipped = 0
    with tempfile.TemporaryDirectory() as tmp:
        for source in sources:
            row, status = matrix_lookup(source)
            if not args.strict and (row is None or status != "implemented"):
                why = (
                    "no capability row for this case"
                    if row is None
                    else f"row {row} is {status}"
                )
                print(f"SKIP {source.stem} ({why})")
                skipped += 1
                continue
            if check(source, args.cc, pathlib.Path(tmp)) == "pass":
                passed += 1
            else:
                failed += 1
    print(f"{passed} passed, {skipped} skipped, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
