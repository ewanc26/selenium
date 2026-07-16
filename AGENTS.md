# AGENTS.md

Guidance for agents working on Selenium, a Python 3.10+ compiler for a lunar/poetic, explicitly typed language that emits C.

## Compiler pipeline

- `lexer.py` recognizes `wax`/`seal`, `ritual`, `eclipse`/`shadow`, `tide`, `orbit`, switch/case, control statements, typed literals, casts, comments, and C-like operators with line/column tokens.
- `parser.py` builds the dataclass AST in `ast.py` for globals, functions, blocks, loops, switch, declarations, assignments, calls, ternaries, casts, and expressions.
- `sema.py` performs two passes so forward function calls work, tracks lexical scopes and expression types, enforces exact types/no implicit coercion, mutability, argument counts, break/continue placement, switch/case rules, and allowed explicit casts.
- `codegen_c.py` emits includes, static I/O wrappers, globals, functions, and a C `main`. It maps `float` to `double`, `bool` to `_Bool`, and `string` to `const char *`; switch cases do not receive implicit `break` statements.
- `main.py` requires `-o` or `--run`, uses argument-based compiler invocation and a temporary directory, reports missing/compiler failures, and propagates the generated program's exit code.
- `examples/` is the only checked-in behavior corpus. There is currently no automated test directory despite the prior guidance.

## Known gaps and invariants

- Preserve keyword grammar, strict type rules, source locations, C escaping, and CLI exit behavior. Keep README examples and all `.sel` fixtures compiling.
- `main.py` catches parse/semantic errors but does not currently catch `LexError` or `CodegenError`; malformed lexemes/codegen gaps can produce tracebacks.
- Semantic analysis does not prove every non-void function returns on all paths. Prefix `++`/`--` checks numeric type but not that the operand is a mutable lvalue, so invalid C may be emitted.
- Top-level initializers are emitted as C globals even when their expression is not a valid constant initializer. Validate or lower such cases before expanding global behavior.
- Built-in readers do not check `scanf` return values; define failure semantics before relying on input correctness.

## Validation

Install with `pip install -e .`, run `python -m compileall selenium`, and add/run pytest coverage for changed behavior. Compile every `examples/*.sel` to C and execute safe examples with GCC; sample Clang too. Cover lexer errors, all scopes/types/casts, missing returns, invalid increment targets, nonconstant globals, switch fallthrough, I/O failure, string/char escaping, compiler-not-found/failure, and child exit propagation. Do not claim a repository test suite until one exists or commit generated binaries/caches.
