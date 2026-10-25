# Selenium compiler

Selenium is a small esoteric language with a lunar / poetic surface and a strict, C-like core.
It compiles to C via a C++ compiler.

> 🧶 Also available on [Tangled](https://tangled.org/ewancroft.uk/selenium)

---

## Installation

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binary is `build/seleniumc`.

---

## Usage

```sh
# Compile to a C file
seleniumc hello.sel -o hello.c

# Compile, run with gcc, and execute immediately
seleniumc hello.sel --run

# Use a different C compiler
seleniumc hello.sel --run --cc clang
```

---

## Language reference

### Types

| Selenium | C equivalent | Notes |
|----------|-------------|-------|
| `int` | `int` | 32-bit integer |
| `float` | `double` | 64-bit float |
| `bool` | `_Bool` | `true` / `false` literals |
| `char` | `char` | single-quoted character |
| `string` | `const char *` | double-quoted literal |
| `void` | `void` | return type only |

### Comments

```selenium
// single line comment
/* block
   comment */
```

### Variables

```selenium
wax int x = 5;       // mutable
seal int y = 10;     // immutable (const)
```

Variables are declared with `wax` (mutable) or `seal` (immutable), followed by type and name.
Top-level declarations become C globals. Declarations inside blocks are local.

Pointers are null with `nullptr`, the C23 null pointer constant. It converts to any pointer or function-pointer type and can be compared with `==` and `!=`. It compiles to `NULL`.

```selenium
frame Node { int value; Node* next; };
wax Node tail = {3, nullptr};
wax Node mid = {2, &tail};              // a global may take the address of an earlier global
wax Node[2] pool = [{1, &pool[1]}, {2, nullptr}];
```

Global initialisers may be constant expressions, string literals, `nullptr`, function names and addresses of globals (`&x`, `&arr[i]`, `&s.field`). A global may refer to its own address in its initialiser, as in `pool` above. A global initialiser that uses a runtime value is assigned in `main`, but a brace or array list needs every element to be constant.

### Functions

```selenium
ritual add(int a, int b) -> int {
    return a + b;
};
```

Functions are defined with `ritual`, take typed parameters, and specify a return type after `->`.

Declare a C function with `summon ritual name(type p) -> type;`. Add `from "file.c"` to compile and link a plain C source file, resolved relative to the `.sel` file.
Recursion is supported. Functions may be called before they are defined.

`ritual` definitions and `summon` declarations are only allowed at top level. Writing one inside a block (a `ritual` body, `eclipse`, `tide`, and so on) is a compile error, as are `frame`, `fusion`, `phase` and `alias` declarations inside blocks.

### I/O

```selenium
whisper expr;              // print any type, followed by newline
wax int x = read_int();    // read int from stdin
wax float f = read_float();
wax bool b = read_bool();
wax char c = read_char();
```

### Control flow

```selenium
// if / else
eclipse (condition) {
    ...
} shadow {
    ...
};

// while
tide (condition) {
    ...
};

// do-while
undertow {
    ...
} tide (condition);

// for
orbit (wax int i = 0; i < 10; i = i + 1) {
    ...
};

// switch
switch (expr) {
    case 1: { whisper "one"; break; };
    case 2: { whisper "two"; break; };
    default: { whisper "other"; break; };
};
```

`break` exits a loop or switch; `continue` skips to the next loop iteration.

### Goto and labels

```selenium
drift skip;
x = 42;
skip: {
    whisper x;
};
```

### Sizeof

```selenium
wax int size = measure(int);
whisper size;

wax int arr = 10;
whisper measure(arr);
```

### Operators

| Category | Operators |
|----------|-----------|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `<` `<=` `>` `>=` `==` `!=` |
| Logical | `&&` `\|\|` `!` |
| Bitwise | `&` `\|` `^` `<<` `>>` |
| Prefix | `++` `--` |
| Ternary | `cond ? then : else` |

### Cast

```selenium
wax float f = cast(float, 5);
wax int i = cast(int, 3.14);
```

Explicit type conversion between numeric types, bool, and char.

---

## Examples

### Hello, World!

```selenium
whisper "Hello, World!";
```

### Functions

```selenium
ritual fact(int n) -> int {
    eclipse (n <= 1) {
        return 1;
    } shadow {
        return n * fact(n - 1);
    };
};

whisper fact(10);
```

### While loop

```selenium
wax int i = 0;
tide (i < 5) {
    whisper i;
    i = i + 1;
};
```

### Ternary

```selenium
seal int a = 3;
seal int b = 7;
wax int max = a > b ? a : b;
whisper max;
```

---

## Project layout


```
CMakeLists.txt            – C++20 compiler and unit-test targets
src/lexer/                – tokenizer and token definitions
src/parser/               – recursive-descent parser and AST construction
src/ast/                  – AST node types
src/sema/                 – semantic analysis (currently incomplete)
src/codegen/              – C source emitter
src/cli/                  – seleniumc command-line driver
tests/unit/               – Catch2 unit tests
tests/differential/       – .sel programs paired with C reference programs
examples/                 – sample .sel programs
```

The documented C++ build target is `build/seleniumc`. The runtime helpers are checked in, but the C++ migration still has gaps: semantic analysis now checks basic variable scopes, loop-control context, undeclared function calls, and call arity, but type compatibility and several other semantic rules remain incomplete; the full differential suite has not been ported to the C++ executable. See [the C++ compiler status](docs/cpp-compiler-status.md) and [issue #14](https://github.com/ewanc26/selenium/issues/14).

## Notes

- The compiler is intentionally strict — no implicit type coercion.
- All types must be declared explicitly.
- Top-level `wax`/`seal` declarations are emitted as C globals; everything else runs in `main`.
- Function definitions can appear anywhere at the top level; forward calls are supported.

## Support

If you find this project useful, consider supporting its development:

[![Ko-fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/ewancroft)
[![GitHub Sponsors](https://img.shields.io/badge/GitHub%20Sponsors-30363D?style=for-the-badge&logo=github&logoColor=white)](https://github.com/sponsors/ewanc26)
