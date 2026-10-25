import pathlib
import shutil
import subprocess
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parent.parent

pytestmark = pytest.mark.skipif(
    shutil.which("clang") is None and shutil.which("gcc") is None,
    reason="no C compiler on PATH",
)


def _cc() -> str:
    return "clang" if shutil.which("clang") else "gcc"


def _run(source: pathlib.Path) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, "-m", "selenium.main", str(source), "--run", "--cc", _cc()],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )


def test_top_level_summon_links_c_source(tmp_path):
    (tmp_path / "helper.c").write_text("int triple(int n) { return n * 3; }\n")
    program = tmp_path / "main.sel"
    program.write_text(
        'summon ritual triple(int n) -> int from "helper.c";\n' "whisper triple(7);\n"
    )
    result = _run(program)
    assert result.returncode == 0, result.stderr
    assert result.stdout == "21\n"


@pytest.mark.parametrize(
    "block",
    [
        "summon ritual abs(int n) -> int;",
        "ritual square(int n) -> int { return n * n; };",
    ],
    ids=["summon", "ritual"],
)
def test_function_declarations_are_rejected_inside_blocks(tmp_path, block):
    # Function declarations and definitions belong at top level; the C they
    # would produce inside a function body is not valid.
    program = tmp_path / "nested.sel"
    program.write_text(
        f"ritual main_program() -> void {{\n    {block}\n}};\nmain_program();\n"
    )
    result = _run(program)
    assert result.returncode == 1
    assert "only allowed at top level" in result.stderr


def test_non_constant_global_element_is_a_clean_error(tmp_path):
    program = tmp_path / "nonconst.sel"
    program.write_text(
        "wax int a = 1;\n"
        "wax int b = a;\n"
        "wax int[2] c = [b, 0];\n"
        "whisper c[0];\n"
    )
    result = _run(program)
    assert result.returncode == 1
    assert "needs constant elements" in result.stderr
    assert "Traceback" not in result.stderr
