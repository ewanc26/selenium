# C++ compiler status

This is an implementation-status note for the C++20 compiler, not a claim that the full language reference is validated end-to-end.

## Verified in the current CI smoke suite

- CMake builds the `seleniumc` compiler and Catch2 unit-test binary.
- `whisper "Hello, World!";` compiles to C and runs through the selected host C compiler.
- Numeric output is covered by the CLI smoke test.
- Generated C embeds the checked-in runtime helper implementation, so a saved C file can be compiled outside the repository without depending on a source-tree include path.
- `--run` uses a unique temporary directory, quotes shell arguments, and cleans temporary files.
- CLI errors for missing source files and unknown options are tested.

## Incomplete or not yet verified

- `src/sema/sema.cpp` now checks variable declarations/references across nested scopes, duplicate variables/parameters, undeclared function calls, duplicate function definitions, conflicting function arities, wrong call argument counts, `return` outside functions, and `break`/`continue` context. Type compatibility, function parameter types, lvalue validity, and full control-flow analysis are still incomplete. Goto targets are checked within function boundaries, and duplicate labels are rejected.
- The full differential suite still points at the removed Python package and has not been ported to the C++ executable.
- The C++ parser/code generator contains broader syntax than the smoke suite currently verifies. Do not treat the README language reference or `capabilities.json` as proof of full feature parity.
- Runtime behavior for arrays, pointers, aggregates, FFI, type conversions, and advanced control flow requires focused C++ end-to-end regression tests before being marked complete.

## Verification

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The current pull request runs these checks in CI. Full semantic analysis and C++ differential coverage remain tracked in [issue #14](https://github.com/ewanc26/selenium/issues/14).
