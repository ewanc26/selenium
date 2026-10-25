#!/usr/bin/env python3
"""End-to-end smoke tests for the C++ Selenium compiler."""

import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


def run(command: list[str], expected_status: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, timeout=30)
    if result.returncode != expected_status:
        raise AssertionError(
            f"{command!r} returned {result.returncode}, expected {expected_status}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: smoke.py <seleniumc> <host-c-compiler>", file=sys.stderr)
        return 2
    compiler = str(Path(sys.argv[1]).resolve())
    host_cc = str(Path(sys.argv[2]).resolve())
    root = Path(__file__).resolve().parents[1]
    example = str(root / "examples" / "hello_world.sel")

    with tempfile.TemporaryDirectory(prefix="selenium smoke ' path ") as directory:
        temp = Path(directory)
        wrapper = temp / "host C compiler's wrapper"
        wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(host_cc) + ' "$@"\n', encoding="utf-8")
        wrapper.chmod(0o755)

        result = run([compiler, example, "--run", "--cc", str(wrapper)])
        if result.stdout != "Hello, World!\n":
            raise AssertionError(f"unexpected hello-world output: {result.stdout!r}")

        numeric = temp / "numeric.sel"
        numeric.write_text("whisper 42;", encoding="utf-8")
        result = run([compiler, str(numeric), "--run", "--cc", str(wrapper)])
        if result.stdout != "42\n":
            raise AssertionError(f"unexpected numeric output: {result.stdout!r}")

        output = temp / "generated output with spaces.c"
        run([compiler, example, "-o", str(output)])
        generated = output.read_text(encoding="utf-8")
        if "selenium_print_string" not in generated or "selenium_runtime.c" not in generated:
            raise AssertionError("generated C does not reference the required runtime helpers")

        missing = run([compiler, str(temp / "missing.sel"), "-o", str(output)], expected_status=1)
        if "cannot read source file" not in missing.stderr:
            raise AssertionError(f"unclear missing-file diagnostic: {missing.stderr!r}")
        invalid = run([compiler, example, "--unknown"], expected_status=1)
        if "unknown option" not in invalid.stderr:
            raise AssertionError(f"unknown option was not rejected: {invalid.stderr!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
