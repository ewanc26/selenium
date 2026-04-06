# Selenium compiler

Selenium is a small esoteric language with a lunar / poetic surface and a strict, C-like core.

## Features

- strongly typed
- semicolon-terminated statements
- functions
- variables and constants
- `eclipse` / `shadow` for if / else
- `tide` for while
- `orbit` for for
- `switch` / `case` / `default` for switch statements
- `break` and `continue` for loops and switch
- prefix `++` and `--` operators
- ternary conditional `?:`
- `whisper` for printing
- `read_int`, `read_float`, `read_bool`, `read_char` for input
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

tide (moon < 10) {
    whisper moon;
    moon = moon + 1;
};

orbit (wax int i = 0; i < 5; i = i + 1) {
    whisper i;
};

switch (moon) {
    case 1: {
        whisper "one";
        break;
    };
    case 3: {
        whisper "three";
        break;
    };
    default: {
        whisper "other";
        break;
    };
};

wax int max = moon > tide ? moon : tide;
whisper max;

wax int input = read_int();
whisper input;

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
