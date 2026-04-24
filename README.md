# Selenium compiler

Selenium is a small esoteric language with a lunar / poetic surface and a strict, C-like core.
It compiles to C via a Python compiler.

> 🧶 Also available on [Tangled](https://tangled.org/ewancroft.uk/selenium)

---

## Installation

```sh
pip install -e .
```

This installs the `seleniumc` command.

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

### Functions

```selenium
ritual add(int a, int b) -> int {
    return a + b;
};
```

Functions are defined with `ritual`, take typed parameters, and specify a return type after `->`.
Recursion is supported. Functions may be called before they are defined.

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
selenium/
  lexer.py      – tokeniser (keywords, literals, operators)
  parser.py     – recursive-descent parser → AST
  ast.py        – AST node types
  sema.py       – type checker and semantic analyser
  codegen_c.py  – C code generator
  main.py       – CLI entry point
examples/       – sample .sel programs
```

## Notes

- The compiler is intentionally strict — no implicit type coercion.
- All types must be declared explicitly.
- Top-level `wax`/`seal` declarations are emitted as C globals; everything else runs in `main`.
- Function definitions can appear anywhere at the top level; forward calls are supported.
