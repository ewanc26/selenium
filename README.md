# Selenium compiler

Selenium is a small esoteric language with a lunar / poetic surface and a strict, C-like core.

## Features

- strongly typed
- semicolon-terminated statements
- functions
- variables and constants
- `if` / `else`
- `while`
- `return`
- `whisper` for printing
- explicit `cast(type, expr)` conversions

## Syntax sketch

```selenium
wax int moon = 3;
seal int tide = 8;

ritual add(int a, int b) -> int {
    return a + b;
};

eclipse (moon < tide) {
    whisper moon;
} shadow {
    whisper tide;
};

whisper add(moon, tide);
```

## Build a C file

```bash
python -m selenium.main examples/hello.sel -o out.c
gcc out.c -o out
./out
```

Or install it as a script:

```bash
pip install -e .
seleniumc examples/hello.sel -o out.c
```

## Notes

- The compiler is intentionally strict.
- No implicit type coercion.
- Top-level `wax`, `seal`, and statements are emitted into `main`.
- Function definitions become normal C functions.
