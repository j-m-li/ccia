# CC90 - ANSI C90 Compiler in C90

agy --conversation=74f9a373-71da-462a-80d8-e56dc0a222e4

**CC90** is a complete, self-contained ANSI C90 (ISO C90 / C89) compiler written strictly in ANSI C90, targeting both **x86_64** (System V AMD64 ABI) and **32-bit x86 / i386** (cdecl ABI) Linux.

This project is released into the **Public Domain** under [The Unlicense](UNLICENSE).

---

## Features

- **Strict ANSI C90 Implementation**:
  - Compiles cleanly with `-std=c90 -pedantic -Wall -Wextra -Werror` with zero warnings.
  - Zero non-standard dependencies; uses only the standard C90 library (`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<ctype.h>`, `<stdarg.h>`).
- **Complete C Preprocessor (CPP)**:
  - Macro definitions (`#define`), object-like and function-like macros with argument substitution.
  - Undefining macros (`#undef`).
  - Conditional compilation directives (`#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`) with full constant expression evaluation (logical, bitwise, comparison, arithmetic, conditional `? :`).
  - File inclusion (`#include <header>` and `#include "file"`).
  - Stringification (`#`) and token concatenation (`##`).
  - Standard predefined macros (`__FILE__`, `__LINE__`, architecture macros).
- **Lexical Analyzer (Lexer)**:
  - All ANSI C90 keywords, multi-character operators, punctuation, comments (`/* ... */`), character escapes, octal/hex/decimal integer literals, floating-point literals, string literal concatenation, and line splicing (`\ \n`).
- **Type System**:
  - Scalar types (`void`, `char`, `short`, `int`, `long`, signed and unsigned variants, `float`, `double`).
  - Derived types: pointers (`*`, `**`), arrays (1D and multidimensional), function pointers, function prototypes.
  - Aggregate types: `struct` (field alignment and offset computation), `union` (memory sharing), `enum` (constant evaluation), and `typedef`.
  - Type decay, integral promotions, and arithmetic type conversions.
- **Parser**:
  - Recursive descent parser with full standard expression precedence.
  - Declarations, complex declarators, nested initializers (scalar, array, struct).
  - Statements: `if`/`else`, `while`, `do..while`, `for`, `switch`/`case`/`default`, `goto`, labels, `break`, `continue`, `return`.
- **x86_64 Code Generator (`gen.c`)**:
  - Emits standard 64-bit GNU assembler syntax (`.s`).
  - System V AMD64 ABI compliance: register arguments (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`), stack arguments (arguments 7+), 16-byte aligned stack frames.
- **x86 32-bit (i386) Code Generator (`gen_i386.c`)**:
  - Emits standard 32-bit GNU assembler syntax (`.s`).
  - Standard cdecl ABI compliance: stack-based argument passing (`8(%ebp)+`), caller cleanup, `%eax` return values, 32-bit pointer and `long` arithmetic.
- **Compiler Drivers**:
  - `cc90`: Native x86_64 compiler driver.
  - `cc90-i386`: Native 32-bit x86 / i386 compiler driver (`as --32`, `gcc -m32`).
  - Standard CLI supporting `-o`, `-S`, `-c`, `-E`, `-I`, `-D`, `-v`, `-h`.

---

## Directory Structure

```
.
├── include/
│   └── c90.h               # Core header (AST, Tokens, Types, Symbols, Driver API)
├── src/
│   ├── main.c              # CLI Driver and compiler entry point
│   ├── util.c              # Safe allocators, Vector, Map, StrBuf, Error handler
│   ├── lex.c               # Lexer / Tokenizer
│   ├── cpp.c               # C Preprocessor and macro evaluation engine
│   ├── type.c              # Type system and struct/union layout
│   ├── sym.c               # Lexical scope and symbol tables
│   ├── parse.c             # Recursive descent parser and constant folding
│   ├── gen.c               # x86_64 Code Generator
│   └── gen_i386.c          # x86 32-bit (i386) Code Generator
├── tests/
│   ├── test_expr.c         # Arithmetic, bitwise, comparison, logic test
│   ├── test_control.c      # if/else, loops, switch/case, goto test
│   ├── test_types.c        # Scalars, arrays, pointers, casts, sizeof test
│   ├── test_structs.c      # Structs, unions, enums, typedefs test
│   ├── test_functions.c    # ABI argument passing, recursion, function pointers test
│   ├── test_preprocessor.c # Macros, stringify, concat, conditionals test
│   ├── test_libc.c         # malloc/free, qsort, string functions, stdio test
│   ├── test_comprehensive.c# Binary Search Tree, Sieve of Eratosthenes, Matrix mult test
│   └── run_tests.sh        # Automated test suite runner
├── Makefile                # Multi-target build and bootstrap configuration
└── UNLICENSE               # Public Domain dedication
```

---

## Building

Build both the x86_64 (`cc90`) and 32-bit x86 (`cc90-i386`) compilers:

```bash
make
```

To build in strict ISO C90 mode with all warnings enabled as errors:

```bash
make clean && CFLAGS="-std=c90 -pedantic -Wall -Wextra -Werror -O2 -Iinclude" make
```

---

## Self-Hosting / Bootstrap

Both **cc90** and **cc90-i386** are fully self-hosting (can compile themselves). You can verify 3-stage self-compilation and fixed-point reproducibility:

```bash
# Verify 3-stage self-compilation for x86_64:
make self

# Verify 3-stage self-compilation for 32-bit i386:
make self-i386

# Run full test suite across all 3 compiler stages for both architectures:
make test-self
```

The bootstrap process ensures:
1. **Stage 1 (`cc90` / `cc90-i386`)**: Compiled from `src/*.c` using the host C compiler.
2. **Stage 2**: Compiled from `src/*.c` using Stage 1.
3. **Stage 3**: Compiled from `src/*.c` using Stage 2.
4. **Fixed-Point Verification**: Stage 2 and Stage 3 emit 100% byte-for-byte identical assembly across the entire compiler source codebase.

---

## Running Tests

Execute the automated test suite on both targets:

```bash
make test
```

Or test specific targets:

```bash
make test-x86_64
make test-i386
```

---

## Usage

### 64-bit (x86_64):
```bash
./cc90 -o myprog myprog.c
./cc90 -S -o myprog.s myprog.c
```

### 32-bit (i386):
```bash
./cc90-i386 -o myprog32 myprog.c
./cc90-i386 -S -o myprog32.s myprog.c
```

### Compile to an object file (`.o`):
```bash
./cc90 -c -o myprog.o myprog.c
```

### Preprocess only (print to stdout):
```bash
./cc90 -E myprog.c
```

### Include directories and Macro definitions:
```bash
./cc90 -Iinclude -DDEBUG=1 -o myprog myprog.c
```

---

## License

This is free and unencumbered software released into the **Public Domain**.
See the [UNLICENSE](UNLICENSE) file or <https://unlicense.org/> for details.
