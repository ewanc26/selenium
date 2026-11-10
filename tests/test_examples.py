import pathlib
import shutil
import subprocess
import sys

import capability
import pytest

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLES = sorted((ROOT / "examples").glob("*.sel"))

# Examples whose program depends on a capability row that is not yet
# implemented in the C++ port are skipped with the matrix status.
EXAMPLE_ROW = {
    "for": "control.while-for",
    "switch": "control.switch",
}

EXPECTED_OUTPUT = {
    "hello_world": "Hello, World!\n",
    "factorial": "120\n",
    "char": "A\n65\n",
}

pytestmark = pytest.mark.skipif(
    shutil.which("clang") is None and shutil.which("gcc") is None,
    reason="no C compiler on PATH",
)


def _cc() -> str:
    return "clang" if shutil.which("clang") else "gcc"


def _run_example(path: pathlib.Path) -> subprocess.CompletedProcess:
    seleniumc = ROOT / "build" / "seleniumc"
    return subprocess.run(
        [str(seleniumc), str(path), "--run", "--cc", _cc()],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )


@pytest.mark.parametrize("path", EXAMPLES, ids=lambda p: p.stem)
def test_example_compiles_and_runs(path):
    row = EXAMPLE_ROW.get(path.stem)
    if row and capability.status(row) != "implemented":
        pytest.skip(f"capability row {row} is {capability.status(row)}")
    result = _run_example(path)
    assert result.returncode == 0, result.stderr or result.stdout


@pytest.mark.parametrize("name", sorted(EXPECTED_OUTPUT))
def test_example_output(name):
    result = _run_example(ROOT / "examples" / f"{name}.sel")
    assert result.returncode == 0, result.stderr
    assert result.stdout == EXPECTED_OUTPUT[name]
