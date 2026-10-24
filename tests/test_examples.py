import pathlib
import shutil
import subprocess
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parent.parent
EXAMPLES = sorted((ROOT / "examples").glob("*.sel"))

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
    return subprocess.run(
        [sys.executable, "-m", "selenium.main", str(path), "--run", "--cc", _cc()],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )


@pytest.mark.parametrize("path", EXAMPLES, ids=lambda p: p.stem)
def test_example_compiles_and_runs(path):
    result = _run_example(path)
    assert result.returncode == 0, result.stderr or result.stdout


@pytest.mark.parametrize("name", sorted(EXPECTED_OUTPUT))
def test_example_output(name):
    result = _run_example(ROOT / "examples" / f"{name}.sel")
    assert result.returncode == 0, result.stderr
    assert result.stdout == EXPECTED_OUTPUT[name]
