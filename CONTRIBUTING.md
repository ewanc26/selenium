# Contributing to Selenium

Selenium is a C++20 compiler that lowers the .sel language to generated C.

## Development setup

A C++20 compiler, CMake, and a host C compiler are required. Catch2 is fetched by CMake during configuration.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
./build/unit_tests
```

The current unit-test suite is not yet green on main: the parser hello-world test fails. End-to-end differential testing must be ported from the old Python module entry point to build/seleniumc; see https://github.com/ewanc26/selenium/issues/14.

## Working rules

- Read the README, CMake build definition, compiler sources, and CI workflow before making changes.
- Preserve the lexer → parser → semantic analysis → C code generation pipeline.
- Update unit or differential tests when behavior changes.
- Do not claim a feature works merely because it appears in the language reference or capabilities.json.
- Keep README.md, AGENTS.md, CONTRIBUTING.md, CI, runtime paths, and the capability matrix aligned with the source that actually builds.
- Keep changes focused, and use a feature branch and pull request.
- Report the exact build and test commands run and their results.