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


FRAME_QUADS = 656  # canonical grid: ~41 ring slices x ~16 tube steps per frame
FRAMES = 48


def test_donut_engine_token_stream():
    # The live engine emits a finite F/P/S token stream the SDL2 host replays.
    result = _run_example(ROOT / "examples" / "donut_engine.sel")
    assert result.returncode == 0, result.stderr
    toks = result.stdout.split()
    # First frame is F, last token is S; every P carries 11 numeric arguments.
    assert toks[0] == "F" and toks[-1] == "S"
    frames = quads = 0
    frame_quads = 0
    for t in toks:
        if t == "F":
            assert frame_quads == FRAME_QUADS or frames == 0
            frames += 1
            frame_quads = 0
        elif t == "P":
            quads += 1
            frame_quads += 1
    assert frames == FRAMES and quads == FRAMES * FRAME_QUADS, (frames, quads)
    # Every P is followed by exactly 11 floats before the next token.
    i = 0
    while i < len(toks):
        if toks[i] == "P":
            for t in toks[i + 1 : i + 12]:
                float(t)
            i += 12
        else:
            assert toks[i] in ("F", "S"), toks[i]
            i += 1


def test_donut_engine_generator_is_fresh():
    # Regenerating the engine example reproduces the committed file exactly.
    import subprocess

    result = subprocess.run(
        [sys.executable, str(ROOT / "scripts" / "gen_donut_engine.py")],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert result.returncode == 0, result.stderr
    assert not subprocess.run(
        ["git", "diff", "--quiet", "examples/donut_engine.sel"], cwd=ROOT
    ).returncode


@pytest.mark.skipif(shutil.which("sdl2-config") is None, reason="no SDL2 on PATH")
def test_donut_engine_pipeline_headless_render():
    # Engine stdout piped into the SDL2 host renders N frames headlessly.
    cc = _cc()
    host_bin = ROOT / "build" / "donut_host"
    if not host_bin.exists():
        subprocess.run(
            f"cc -std=c99 -O2 $(sdl2-config --cflags --libs) -o {host_bin} host/donut_host.c",
            cwd=ROOT,
            shell=True,
            check=True,
            capture_output=True,
            timeout=120,
        )
    engine = subprocess.run(
        [
            str(ROOT / "build" / "seleniumc"),
            str(ROOT / "examples" / "donut_engine.sel"),
            "--run",
            "--cc",
            cc,
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert engine.returncode == 0, engine.stderr
    host = subprocess.run(
        [str(host_bin), "--frames", "3"],
        cwd=ROOT,
        input=engine.stdout,
        capture_output=True,
        text=True,
        timeout=120,
        env={**__import__("os").environ, "SDL_VIDEODRIVER": "dummy"},
    )
    assert host.returncode == 0, host.stderr
    assert "frames" in host.stderr and "quads" in host.stderr
    frames, quads = host.stderr.strip().split(": ")[1].split(" frames, ")
    assert int(frames) == FRAMES and int(quads.split()[0]) == FRAMES * FRAME_QUADS


def test_donut_engine_matches_canonical_frames():
    # Cross-language harmonisation: the engine's first 8 frames are
    # byte-identical to the canonical sigi stream (vendored so CI needs
    # no sigi checkout; frame 0 of the canonical starts at spin A=0.05).
    result = _run_example(ROOT / "examples" / "donut_engine.sel")
    assert result.returncode == 0, result.stderr
    fixture = (ROOT / "tests" / "fixtures" / "donut_canonical_8f.txt").read_text()
    fixture_lines = fixture.splitlines(keepends=True)
    assert len(fixture_lines) == 8 * (2 + FRAME_QUADS * 12), len(fixture_lines)
    assert result.stdout[: len(fixture)] == fixture
