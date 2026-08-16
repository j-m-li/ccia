#!/bin/bash
#
# This is free and unencumbered software released into the public domain.
# See the UNLICENSE file or http://unlicense.org/ for details.
#

set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$DIR/.." && pwd)"
CC90="$ROOT_DIR/cc90"

echo "=========================================="
echo " Running CC90 ANSI C90 Compiler Test Suite"
echo "=========================================="

TESTS=(
    "test_expr"
    "test_control"
    "test_types"
    "test_structs"
    "test_functions"
    "test_preprocessor"
    "test_libc"
    "test_comprehensive"
)

PASSED=0
TOTAL=${#TESTS[@]}

for t in "${TESTS[@]}"; do
    SRC="$DIR/$t.c"
    BIN="$DIR/$t.bin"
    
    printf "Testing %-20s ... " "$t"
    
    # Compile with cc90
    if ! "$CC90" -o "$BIN" "$SRC" 2> "$DIR/$t.err"; then
        echo "FAILED (Compilation error)"
        cat "$DIR/$t.err"
        exit 1
    fi
    
    # Execute compiled binary
    if ! "$BIN" > "$DIR/$t.out" 2>&1; then
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
