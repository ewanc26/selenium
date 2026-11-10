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
    "primes": "\n".join(["2", "3", "5", "7", "11", "13", "17", "19", "23", "29"])
    + "\n",
    "fizzbuzz": "\n".join(
        [
            "1",
            "2",
            "Fizz",
            "4",
            "Buzz",
            "Fizz",
            "7",
            "8",
            "Fizz",
            "Buzz",
            "11",
            "Fizz",
            "13",
            "14",
            "FizzBuzz",
        ]
    )
    + "\n",
}


def test_donut_renders_sixty_quads_per_frame():
    # The donut demo emits a 4-frame polygonal torus as whitespace-tolerant SVG.
    result = _run_example(ROOT / "examples" / "donut.sel")
    assert result.returncode == 0, result.stderr
    assert result.stdout.count("<polygon") == 640
    assert result.stdout.startswith("<svg") and result.stdout.rstrip().endswith(
        "</svg>"
    )


def test_collatz_trajectory():
    # The example walks the canonical 27 trajectory: 112 values, ending at 1.
    result = _run_example(ROOT / "examples" / "collatz.sel")
    assert result.returncode == 0, result.stderr
    values = result.stdout.split()
    assert len(values) == 112, len(values)
    assert values[0] == "27" and values[-1] == "1"
    for a, b in zip(values, values[1:]):
        if int(a) % 2 == 0:
            assert int(b) == int(a) // 2, (a, b)
        else:
            assert int(b) == 3 * int(a) + 1, (a, b)


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
