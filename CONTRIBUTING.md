# Contributing to Selenium

An esoteric language with a lunar/poetic surface and a strict, C-like core, compiling to C via a C++ compiler (`wax` declares mutable, `seal` declares immutable, and so on down the word set). Thanks for wanting to help!

Not to be confused with the browser automation stack; this one waxes and seals.

## Getting started

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
# binary: build/seleniumc
```

## Notes for this project

- The surface is lunar and poetic; the core is strict and C-like. Keep both: new syntax should read like a moonlit night and lower like C, not drift toward either plain English or widgetry.
- The language reference below the fold in the README is the contract: changes need a reference-table update in the same PR.
- The compiler emits readable C (`seleniumc hello.sel -o hello.c` is meant to be inspected); don't obfuscate the output.
- `--run` with a configurable host compiler (`--cc`) is part of the API: keep it working when the generated code changes shape.

## Before opening a PR

- Build passes and sample programs compile and run (`--run` path included).
- Conventional commits; focused PRs; include sample `.sel` programs exercising the change.
- AI-assisted PRs welcome; stand behind every line and run both paths yourself.

## Licence

See [LICENSE](LICENSE). Contributions accepted under the same terms.