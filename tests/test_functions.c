/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>

int add8(int a, int b, int c, int d, int e, int f, int g, int h) {
    return a + b + c + d + e + f + g + h;
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int is_odd(int n);

int is_even(int n) {
    if (n == 0) return 1;
    return is_odd(n - 1);
}

int is_odd(int n) {
    if (n == 0) return 0;
    return is_even(n - 1);
}

int apply_op(int a, int b, int (*op)(int, int)) {
    return op(a, b);
}

int op_add(int x, int y) { return x + y; }
int op_mul(int x, int y) { return x * y; }

int counter(void) {
    static int count = 0;
    count++;
    return count;
}

int test_many_args(void) {
    int res = add8(1, 2, 3, 4, 5, 6, 7, 8);
    if (res != 36) return 1;
    return 0;
}

int test_recursion(void) {
    if (factorial(5) != 120) return 1;
    if (factorial(6) != 720) return 2;

    if (fibonacci(7) != 13) return 3;
    if (fibonacci(10) != 55) return 4;

    if (!is_even(10)) return 5;
    if (is_odd(10)) return 6;
    if (!is_odd(9)) return 7;
    if (is_even(9)) return 8;

    return 0;
}

int test_function_pointers(void) {
    int (*fp)(int, int);

    fp = op_add;
    if (fp(10, 20) != 30) return 1;

    fp = op_mul;
    if (fp(10, 20) != 200) return 2;

    if (apply_op(5, 7, op_add) != 12) return 3;
    if (apply_op(5, 7, op_mul) != 35) return 4;

    return 0;
}

int test_static_locals(void) {
    if (counter() != 1) return 1;
    if (counter() != 2) return 2;
    if (counter() != 3) return 3;
    return 0;
}

int main(void) {
    if (test_many_args() != 0) return 1;
    if (test_recursion() != 0) return 2;
    if (test_function_pointers() != 0) return 3;
    if (test_static_locals() != 0) return 4;

    printf("PASS: test_functions\n");
    return 0;
}
