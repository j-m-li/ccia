#!/bin/bash
#
# This is free and unencumbered software released into the public domain.
# See the UNLICENSE file or http://unlicense.org/ for details.
#

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
CCIA="${CCIA:-${CC90:-$ROOT_DIR/ccia}}"
if [ -z "$CCIA_INCLUDES" ]; then
    CCIA_INCLUDES="${CC90_INCLUDES:--I$ROOT_DIR/include $(for d in /usr/lib64/clang/*/include /usr/lib/clang/*/include /usr/lib/gcc/x86_64-redhat-linux/*/include /usr/lib/gcc/x86_64-linux-gnu/*/include /usr/include/x86_64-linux-gnu /usr/local/include /usr/include; do [ -d "$d" ] && echo "-I$d"; done)}"
fi

echo "=========================================="
echo " Running CCIA ANSI C90 Compiler Test Suite"
echo "=========================================="

TESTS=(
    "test_expr"
    "test_control"
    "test_types"
    "test_structs"
    "test_functions"
    "test_preprocessor"
    "test_libc"
    "test_float"
    "test_bitfields"
    "test_precedence"
    "test_comprehensive"
    "test_stb_image"
)

PASSED=0
TOTAL=${#TESTS[@]}

for t in "${TESTS[@]}"; do
    SRC="$DIR/$t.c"
    BIN="$DIR/$t.bin"
    
    printf "Testing %-20s ... " "$t"
    
    # Compile with ccia
    if [ -n "$CCIA_COMPILER_RUNNER" ]; then
        if ! $CCIA_COMPILER_RUNNER $CCIA $CCIA_INCLUDES -S -o "$DIR/$t.tmp.s" "$SRC" 2> "$DIR/$t.err"; then
            echo "FAILED (Compilation error)"
            cat "$DIR/$t.err"
            exit 1
        fi
        clang --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -fuse-ld=lld -nostdlib -static -o "$BIN" "$DIR/$t.tmp.s" "$ROOT_DIR/src/runtime_rv32.rv32i.o" "$ROOT_DIR/src/softfloat.rv32i.o" 2>> "$DIR/$t.err" || {
            echo "FAILED (Linking error)"
            cat "$DIR/$t.err"
            exit 1
        }
        rm -f "$DIR/$t.tmp.s"
    else
        if ! $CCIA $CCIA_INCLUDES -o "$BIN" "$SRC" 2> "$DIR/$t.err"; then
            echo "FAILED (Compilation error)"
            cat "$DIR/$t.err"
            exit 1
        fi
    fi
    
    # Execute compiled binary
    if ! $CCIA_RUNNER "$BIN" > "$DIR/$t.out" 2>&1; then
        echo "FAILED (Execution error)"
        cat "$DIR/$t.out"
        exit 1
    fi
    
    echo "OK"
    PASSED=$((PASSED + 1))
    rm -f "$BIN" "$DIR/$t.out" "$DIR/$t.err" "$DIR/$t.s" "$DIR/$t.o"
done

echo "=========================================="
echo " All $PASSED / $TOTAL tests passed successfully!"
echo "=========================================="
