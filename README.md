# CCIA - ANSI C90 (ISO C89) Compiler in Pure C90

agy --conversation=74f9a373-71da-462a-80d8-e56dc0a222e4

**CCIA** is a complete, self-contained, self-hosting ANSI C90 (ISO/IEC 9899:1990, commonly referred to as C89) compiler written strictly in portable ANSI C90. It targets **x86_64** (System V AMD64 ABI), **32-bit x86 / i386** (cdecl ABI), and **32-bit RISC-V RV32I** (ILP32 ABI) Linux using Clang/LLD.

This project is dedicated to the **Public Domain** under [The Unlicense](UNLICENSE).

---

## Highlights

- **Pure ANSI C90 Standard Compliance**: Written entirely in ISO C90. Builds cleanly under `-std=c90 -pedantic -Wall -Wextra -Werror` with zero warnings.
- **Zero External Dependencies**: Relies exclusively on standard C90 library headers (`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<ctype.h>`, `<stdarg.h>`).
- **3-Stage Self-Hosting Bootstrap**: Verified 3-stage bootstrap fixed-point on both x86_64 (`make self`) and 32-bit i386 (`make self-i386`). Stage 2 and Stage 3 produce **100% byte-for-byte identical assembly**.
- **Real-World Capability**: Capable of compiling complex real-world single-header C libraries, such as `stb_image.h` (decoding PNG) and `stb_image_write.h` (encoding JPEG).
- **Multiple Target Architectures**:
  - **x86_64**: Linux System V AMD64 ABI
  - **i386**: Linux 32-bit x86 cdecl ABI
  - **RV32I**: Linux 32-bit RISC-V ILP32 ABI (pure base integer instruction set assembled and linked via Clang/LLD, tested with `qemu-riscv32-static`)
- **Pure C90 Software Floating-Point Subsystem**: Includes complete IEEE 754 floating-point implementation (`softfloat.c`) for single, double, and long double operations as well as compile-time constant expression evaluation.

---

## Compiler Architecture & Pipeline

```
  Source Code (.c)
         │
         ▼
┌─────────────────┐
│ C Preprocessor  │  Macro expansion, #include, conditional compilation (#if, #ifdef),
│     (cpp.c)     │  stringification (#), token concatenation (##)
└────────┬────────┘
         ▼
┌─────────────────┐
│ Lexer / Scanner │  ANSI C90 tokens, keyword recognition, numeric literals,
│     (lex.c)     │  string concatenation, character escape sequences
└────────┬────────┘
         ▼
┌─────────────────┐
│ Parser & AST    │  Recursive descent parser, complete operator precedence climbing,
│    (parse.c)    │  declarations, compound initializers, control-flow constructs
└────────┬────────┘
         ▼
┌─────────────────┐
│  Type Checker   │  Type synthesis, implicit promotions, arithmetic conversions,
│ (type.c, sym.c) │  struct/union/enum layouts, bit-fields, symbol scoping
└────────┬────────┘
         ▼
┌────────────────────────────────────────────────────────────────────────┐
│                            Code Generators                             │
│  ┌────────────────────────┬──────────────────────┬──────────────────┐  │
│  │ x86_64 Backend (gen.c) │ i386 Backend (gen... │ RV32I Backend    │  │
│  │ System V AMD64 ABI     │ cdecl ABI            │ ILP32 ABI        │  │
│  │ Register calling conv. │ Stack calling conv.  │ Base RV32I ISA   │  │
│  │ 16-byte aligned frames │ 32-bit pointer model │ a0..a7 registers │  │
│  └────────────────────────┴──────────────────────┴──────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    ▼
                           Assembly Code (.s)
                                    │
                                    ▼
                       Assembler & Linker Driver
                        (as / ld / clang / lld)
                                    │
                                    ▼
                             Executable (ELF)
```

---

## Key Components

### 1. Preprocessor (`src/cpp.c`)
- Complete macro expansion engine supporting both object-like (`#define FOO 1`) and function-like (`#define MAX(a, b) ((a) > (b) ? (a) : (b))`) macros.
- Full support for stringification (`#`), token concatenation (`##`), and variable argument lists (`...` / `__VA_ARGS__`).
- Conditional compilation (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`) with full constant expression evaluation (logical, bitwise, comparison, arithmetic, and ternary conditional operators).
- Standard include file resolution (`#include <header>` and `#include "file"`), line markers, and built-in predefined macros (`__FILE__`, `__LINE__`, `__DATE__`, `__TIME__`, `__STDC__`, `__CCIA__`, `__CCIA_VERSION__`).

### 2. Lexer (`src/lex.c`)
- Full tokenization conforming to ANSI C90 specifications.
- Handles multi-character operators (`++`, `--`, `->`, `<<`, `>>`, `<=`, `>=`, `==`, `!=`, `&&`, `||`, compound assignments).
- String literal concatenation and character escape sequences (`\n`, `\t`, `\r`, `\0`, `\xHH`, `\OOO`).
- Decodes integer constants (decimal, octal, hexadecimal, suffixes `U`, `L`, `UL`) and floating-point literals (`float`, `double`, `long double`).

### 3. Parser & AST (`src/parse.c`)
- Pure recursive descent parser generating a strongly typed abstract syntax tree (AST).
- Complete standard operator precedence climbing matching ANSI C90 specifications.
- Handles complex declarations, pointer declarators, multidimensional arrays, and function pointers.
- Full support for nested compound initializers for arrays, structs, and unions with automatic zero-padding.
- Statements: `if`/`else`, `switch`/`case`/`default`, `while`, `do..while`, `for`, `goto`, labels, `break`, `continue`, `return`.

### 4. Type System & Symbol Table (`src/type.c`, `src/sym.c`)
- Complete representation of primitive and derived types (`void`, `char`, `short`, `int`, `long`, `float`, `double`, `long double`, signed/unsigned variants, pointers, arrays, functions, structs, unions, enums, typedefs).
- Standard integer promotions and usual arithmetic conversions.
- Struct/union memory layout computation, field alignment, padding, and bit-field packing.
- Multi-level lexical scoping with distinct namespaces for symbols, struct/union/enum tags, and labels.

### 5. Software Floating-Point Engine (`src/softfloat.c`)
- Written strictly in ANSI C90 without requiring compiler runtime floating-point support.
- Implements IEEE 754 single-precision (32-bit), double-precision (64-bit), and extended/quad-precision (80-bit / 128-bit) arithmetic: addition, subtraction, multiplication, division, comparisons, and conversions.
- Evaluates constant floating-point expressions at compile-time for global/static initializers.

### 6. Target Code Generation
- **x86_64 (`src/gen.c`)**: System V AMD64 ABI compliant. Passes arguments in registers (`%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9`), uses 16-byte stack frame alignment, supports variable argument functions (`__builtin_va_start`, `__builtin_va_arg`, `__builtin_va_copy`), and produces standard GNU assembler output.
- **i386 (`src/gen_i386.c`)**: Standard Linux cdecl ABI compliant. Stack-based argument passing, caller cleanup, `%eax` return values, and 32-bit pointer/integer arithmetic.
- **RV32I (`src/gen_riscv32.c`)**: Standard Linux RISC-V 32-bit ILP32 ABI compliant. Pure base RV32I integer instruction set, argument passing in `a0..a7`, frame pointer `s0`, software integer division/multiplication and floating point emulation, self-contained Linux runtime (`src/runtime_rv32.c`), freestanding headers in `include/riscv32/`, assembled and linked via Clang (`clang --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -fuse-ld=lld`).

---

## Directory Structure

```
.
├── include/
│   ├── c90.h               # Core compiler definitions (AST, Tokens, Types, Symbols, Driver API)
│   ├── softfloat.h         # Software floating-point subsystem declarations
│   └── riscv32/            # Freestanding C90 standard headers for 32-bit RISC-V target
│       ├── assert.h
│       ├── ctype.h
│       ├── limits.h
│       ├── math.h
│       ├── stdarg.h
│       ├── stddef.h
│       ├── stdint.h
│       ├── stdio.h
│       ├── stdlib.h
│       └── string.h
├── src/
│   ├── main.c              # Compiler driver entry point and CLI option handling
│   ├── util.c              # Safe allocators, dynamic Vector, Map, StrBuf, diagnostics
│   ├── lex.c               # Lexer / Tokenizer
│   ├── cpp.c               # C Preprocessor and macro evaluation engine
│   ├── type.c              # Type system, struct/union layout, type promotions
│   ├── sym.c               # Lexical scope and symbol table management
│   ├── parse.c             # Recursive descent parser and constant folding
│   ├── gen.c               # x86_64 Code Generator (System V AMD64 ABI)
│   ├── gen_i386.c          # 32-bit x86 Code Generator (cdecl ABI)
│   ├── gen_riscv32.c       # 32-bit RISC-V RV32I Code Generator (ILP32 ABI)
│   ├── runtime_rv32.c      # Freestanding Linux C runtime for RV32I (I/O, memory, math, syscalls)
│   └── softfloat.c         # Pure C90 IEEE 754 software floating-point implementation
├── tests/
│   ├── test_expr.c         # Expressions, arithmetic, bitwise, and logical operators
│   ├── test_control.c      # Control flow: if/else, while, do..while, for, switch, goto
│   ├── test_types.c        # Primitive types, arrays, pointers, casts, sizeof
│   ├── test_structs.c      # Structs, nested structs, unions, enums, typedefs
│   ├── test_functions.c    # Function calls, recursion, argument passing, function pointers
│   ├── test_preprocessor.c # Macros, stringify, token pasting, conditionals, includes
│   ├── test_libc.c         # C standard library integration (malloc/free, qsort, string, stdio)
│   ├── test_float.c        # Floating-point arithmetic, conversions, and math operations
│   ├── test_bitfields.c    # Struct bit-fields layout, extraction, and insertion
│   ├── test_precedence.c   # Operator precedence and associativity validation
│   ├── test_comprehensive.c# Complex algorithms: BST, Sieve of Eratosthenes, Matrix multiply
│   ├── test_stb_image.c    # Real-world image processing (stb_image PNG and stb_image_write JPEG)
│   └── run_tests.sh        # Automated test suite execution script
├── Makefile                # Build system, bootstrap verification, and test targets
└── UNLICENSE               # Public Domain dedication
```

---

## Building

### Quick Build
To compile the x86_64 (`ccia`), 32-bit x86 (`ccia-i386`), and 32-bit RISC-V (`ccia-rv32i`) compilers:

```bash
make
```

### Strict ISO C90 Build
To verify strict standard compliance with all warnings treated as errors:

```bash
make clean
CFLAGS="-std=c90 -pedantic -Wall -Wextra -Werror -O2 -Iinclude" make
```

---

## Self-Hosting & 3-Stage Bootstrap Verification

**CCIA** is fully self-hosting and can compile its own entire codebase from scratch.

```bash
# Verify 3-stage self-compilation for x86_64:
make self

# Verify 3-stage self-compilation for 32-bit i386:
make self-i386

# Run full test suite across all 3 stages on both architectures:
make test-self
```

### How the Bootstrap Works:
1. **Stage 1 (`ccia` / `ccia-i386`)**: Built from `src/*.c` using the host C compiler (e.g., GCC or Clang).
2. **Stage 2 (`ccia_stage2` / `ccia-i386_stage2`)**: Built from `src/*.c` using the Stage 1 compiler.
3. **Stage 3 (`ccia_stage3` / `ccia-i386_stage3`)**: Built from `src/*.c` using the Stage 2 compiler.
4. **Fixed-Point Proof**: Stage 2 and Stage 3 compilers compile every single source file (`src/*.c`) into assembly (`.s`). The resulting assembly files are verified with `diff -u` to be **100% byte-for-byte identical**.

---

## Running Tests

Run the full automated test suite across all target architectures:

```bash
make test
```

Target-specific test execution:
```bash
make test-x86_64   # Test 64-bit compiler (native)
make test-i386     # Test 32-bit x86 compiler (native 32-bit)
make test-rv32i    # Test 32-bit RISC-V RV32I compiler (under qemu-riscv32-static)
```

---

## Command-Line Usage

```
Usage: ccia [options] <input-file.c>

Options:
  -o <file>       Place the output into <file>
  -S              Compile only; generate assembly (.s)
  -c              Compile and assemble, but do not link (.o)
  -E              Preprocess only; print preprocessed source to stdout
  -I <dir>        Add directory to include search path
  -D <macro>[=v]  Define preprocessor macro with optional value
  -v, --version   Display compiler version and public domain notice
  -h, --help      Display available command-line options
```

### Examples

#### 1. Compile and link a program (x86_64):
```bash
./ccia -o hello hello.c
./hello
```

#### 2. Compile and link for 32-bit x86:
```bash
./ccia-i386 -o hello32 hello.c
./hello32
```

#### 3. Compile and link for 32-bit RISC-V (RV32I):
```bash
./ccia-rv32i -Iinclude/riscv32 -Iinclude -o hello_rv32.bin hello.c
qemu-riscv32-static hello_rv32.bin
```

#### 4. Generate assembly output (`.s`):
```bash
./ccia -S -o program.s program.c
```

#### 4. Compile to an object file (`.o`):
```bash
./ccia -c -o program.o program.c
```

#### 5. Preprocess only (print to stdout):
```bash
./ccia -E -Iinclude program.c
```

#### 6. Pass include paths and macro definitions:
```bash
./ccia -Iinclude -DNDEBUG -DVERSION=\"1.0.0\" -o myapp main.c
```

---

## License

This is free and unencumbered software released into the **Public Domain**.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this software, either in source code form or as a compiled binary, for any purpose, commercial or non-commercial, and by any means.

For more information, please refer to the [UNLICENSE](UNLICENSE) file or <https://unlicense.org/>.
