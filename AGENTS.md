# AGENTS.md

Guidance for agents working on Selenium, a Python compiler for a lunar/poetic strongly typed language that emits C.

## Compiler contracts

- `selenium/` contains lexer, parser, type checking, C emission, CLI, and diagnostics.
- `examples/` contains language fixtures; `.sel` syntax and the documented C-like semantics are public behavior.
- Generated C must be portable and deterministic.

## Invariants

- Preserve source positions and distinguish lexical, parse, name, type, and codegen failures.
- Enforce declared types, function signatures, control-flow validity, casts, and return behavior before C emission.
- Escape strings/chars safely and never turn source text into a shell command.
- `--run` must invoke compilers via argument arrays, propagate exit codes, use safe temporary paths, and clean up.
- Avoid undefined C for invalid programs; fail with an actionable source diagnostic instead.

## Validation

Install with `pip install -e .`, run the repository tests, and run `python -m compileall selenium`. Compile and execute representative examples with GCC and Clang where available. Cover malformed tokens, type errors, nested control flow, recursion, I/O, escaping, Unicode policy, compiler-not-found, and runtime exit propagation. Do not commit generated binaries or caches.
