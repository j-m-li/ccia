/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>

int test_scalars(void) {
    char c = 120;
    unsigned char uc = 250;
    short s = -1000;
    unsigned short us = 50000;
    int i = -100000;
    unsigned int ui = 3000000000U;
#ifdef __i386__
    long l = -2000000000L;
    unsigned long ul = 4000000000UL;
#else
    long l = -5000000000L;
    unsigned long ul = 10000000000UL;
#endif

    if (sizeof(char) != 1) { printf("fail 1\n"); return 1; }
    if (sizeof(short) != 2) { printf("fail 2\n"); return 2; }
    if (sizeof(int) != 4) { printf("fail 3\n"); return 3; }
#ifdef __i386__
    if (sizeof(long) != 4) { printf("fail 4\n"); return 4; }
    if (sizeof(void *) != 4) { printf("fail 5\n"); return 5; }
#else
    if (sizeof(long) != 8) { printf("fail 4\n"); return 4; }
    if (sizeof(void *) != 8) { printf("fail 5\n"); return 5; }
#endif

    if (c + 5 != 125) { printf("fail 6: %d\n", c + 5); return 6; }
    if (uc + 5 != 255) { printf("fail 7: %d\n", uc + 5); return 7; }
    if (s + 1000 != 0) { printf("fail 8: %d\n", s + 1000); return 8; }
    if (us - 10000 != 40000) { printf("fail 9: %d\n", us - 10000); return 9; }
    if (i + 100000 != 0) { printf("fail 10: %d\n", i + 100000); return 10; }
    if (ui != 3000000000U) { printf("fail 11\n"); return 11; }
#ifdef __i386__
    if (l != -2000000000L) { printf("fail 12: l=%ld\n", l); return 12; }
    if (ul != 4000000000UL) { printf("fail 13\n"); return 13; }
#else
    if (l != -5000000000L) { printf("fail 12: l=%ld, expected=%ld\n", l, -5000000000L); return 12; }
    if (ul != 10000000000UL) { printf("fail 13\n"); return 13; }
#endif

    return 0;
}

int test_pointers(void) {
    int val = 42;
    int *p = &val;
    int **pp = &p;

    if (*p != 42) return 1;
    if (**pp != 42) return 2;

    *p = 100;
    if (val != 100) return 3;

    **pp = 200;
    if (val != 200) return 4;

    return 0;
}

int test_arrays(void) {
    int arr[5];
    int i;
    int matrix[3][4];
    int r, c;

    for (i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }
    for (i = 0; i < 5; i++) {
        if (arr[i] != i * 10) return 1;
        if (*(arr + i) != i * 10) return 2;
    }

    if (sizeof(arr) != 5 * sizeof(int)) return 3;

    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            matrix[r][c] = r * 10 + c;
        }
    }
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 4; c++) {
            if (matrix[r][c] != r * 10 + c) return 4;
        }
    }

    return 0;
}

int test_casts(void) {
#ifdef __i386__
    long l = 0x5678ABCD;
#else
    long l = 0x12345678ABCD;
#endif
    int i = (int)l;
    short s = (short)i;
    char c = (char)s;

    if (i != (int)0x5678ABCD) return 1;
    if (s != (short)0xABCD) return 2;
    if (c != (char)0xCD) return 3;

    return 0;
}

int main(void) {
    if (test_scalars() != 0) return 1;
    if (test_pointers() != 0) return 2;
    if (test_arrays() != 0) return 3;
    if (test_casts() != 0) return 4;

    printf("PASS: test_types\n");
    return 0;
}
