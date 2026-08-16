/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <string.h>

#define CONST_VAL 42
#define SQUARE(x) ((x) * (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define STR(s) #s
#define CONCAT(a, b) a ## b

#define LEVEL_1 10
#define LEVEL_2 20

#if LEVEL_1 + LEVEL_2 == 30
#  define COND_SUM_OK 1
#else
#  define COND_SUM_OK 0
#endif

#if (1 << 4) == 16 && (0xFF & 0x0F) == 0x0F
#  define BITWISE_OK 1
#else
#  define BITWISE_OK 0
#endif

#if 0
#  define SHOULD_NOT_DEF 1
#elif 1
#  define ELIF_OK 1
#else
#  define SHOULD_NOT_DEF 2
#endif

int test_macro_expansion(void) {
    if (CONST_VAL != 42) return 1;
    if (SQUARE(5) != 25) return 2;
    if (SQUARE(1 + 2) != 9) return 3;
    if (MAX(10, 20) != 20) return 4;
    if (MAX(50, 20) != 50) return 5;
    return 0;
}

int test_stringify_and_concat(void) {
    const char *s = STR(hello world);
    int CONCAT(my_, var) = 123;

    if (strcmp(s, "hello world") != 0) return 1;
    if (my_var != 123) return 2;

    return 0;
}

int test_conditionals(void) {
    if (COND_SUM_OK != 1) return 1;
    if (BITWISE_OK != 1) return 2;
    if (ELIF_OK != 1) return 3;

#ifdef SHOULD_NOT_DEF
    return 4;
#endif

#ifndef SHOULD_NOT_DEF
    /* OK */
#else
    return 5;
#endif

    return 0;
}

int main(void) {
    if (test_macro_expansion() != 0) return 1;
    if (test_stringify_and_concat() != 0) return 2;
    if (test_conditionals() != 0) return 3;

    printf("PASS: test_preprocessor\n");
    return 0;
}
