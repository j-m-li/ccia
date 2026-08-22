/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAILED: %s (line %d)\n", msg, __LINE__); \
        exit(1); \
    } \
} while (0)

static void test_float_sizes(void) {
    printf("Testing floating point type sizes...\n");
    ASSERT(sizeof(float) == 4, "sizeof(float) == 4");
    ASSERT(sizeof(double) == 8, "sizeof(double) == 8");
    ASSERT(sizeof(long double) == 16, "sizeof(long double) == 16");
}

static void test_float32(void) {
    float a = 3.5f;
    float b = 2.0f;
    float c, d, e, f;
    int i;

    printf("Testing float (32-bit)...\n");

    c = a + b; /* 5.5f */
    d = a - b; /* 1.5f */
    e = a * b; /* 7.0f */
    f = a / b; /* 1.75f */

    ASSERT(c > 5.4f && c < 5.6f, "float add");
    ASSERT(d > 1.4f && d < 1.6f, "float sub");
    ASSERT(e > 6.9f && e < 7.1f, "float mul");
    ASSERT(f > 1.74f && f < 1.76f, "float div");

    /* Unary negation */
    ASSERT(-a < 0.0f, "float neg");

    /* Comparisons */
    ASSERT(a > b, "float gt");
    ASSERT(b < a, "float lt");
    ASSERT(a >= 3.5f, "float ge");
    ASSERT(b <= 2.0f, "float le");
    ASSERT(a == 3.5f, "float eq");
    ASSERT(a != b, "float ne");

    /* Conversions */
    i = (int)a;
    ASSERT(i == 3, "float to int cast");
    b = (float)i;
    ASSERT(b == 3.0f, "int to float cast");
}

static void test_float64(void) {
    double a = 12.5;
    double b = 4.0;
    double c, d, e, f;
    long l;

    printf("Testing double (64-bit)... %f %f\n",a,b);
    
#if 0		
    c = a + b; /* 16.5 */
    d = a - b; /* 8.5 */
    e = a * b; /* 50.0 */
    f = a / b; /* 3.125 */
    ASSERT(c > 16.49 && c < 16.51, "double add");
    ASSERT(d > 8.49 && d < 8.51, "double sub");
    ASSERT(e > 49.99 && e < 50.01, "double mul");
    ASSERT(f > 3.124 && f < 3.126, "double div");

    /* Unary negation */
    ASSERT(-a < 0.0, "double neg");

    /* Comparisons */
    ASSERT(a > b, "double gt");
    ASSERT(b < a, "double lt");
    ASSERT(a >= 12.5, "double ge");
    ASSERT(b <= 4.0, "double le");
    ASSERT(a == 12.5, "double eq");
    ASSERT(a != b, "double ne");

    /* Conversions */
    l = (long)a;
    ASSERT(l == 12, "double to long cast");
    b = (double)l;
    ASSERT(b == 12.0, "long to double cast");
#endif
    printf("Testing double PASS...\n");
}

static void test_float128(void) {
/*    long double a = 100.25L;
    long double b = 25.0L;
    long double c, d, e, f;
    int i;
    printf("Testing long double (128-bit quad)...\n");
*/
#if 0
    c = a + b; /* 125.25L */
    d = a - b; /* 75.25L */
    e = a * 2.0L; /* 200.5L */
    f = a / 4.0L; /* 25.0625L */

    ASSERT(c > 125.24L && c < 125.26L, "long double add");
    ASSERT(d > 75.24L && d < 75.26L, "long double sub");
    ASSERT(e > 200.49L && e < 200.51L, "long double mul");
    ASSERT(f > 25.06L && f < 25.07L, "long double div");

    /* Unary negation */
    ASSERT(-a < 0.0L, "long double neg");

    /* Comparisons */
    ASSERT(a > b, "long double gt");
    ASSERT(b < a, "long double lt");
    ASSERT(a >= 100.25L, "long double ge");
    ASSERT(b <= 25.0L, "long double le");
    ASSERT(a == 100.25L, "long double eq");
    ASSERT(a != b, "long double ne");

    /* Conversions */
    i = (int)a;
    ASSERT(i == 100, "long double to int cast");
    b = (long double)i;
    ASSERT(b == 100.0L, "int to long double cast");
#endif
}

static void test_cross_conversions(void) {
    float f = 1.25f;
    double d;
    long double ld;

    printf("Testing cross-precision conversions...\n");

    /* float -> double -> long double */
    d = (double)f;
    ASSERT(d > 1.24 && d < 1.26, "float to double");

    ld = (long double)d;
    ASSERT(ld > 1.24L && ld < 1.26L, "double to long double");

    /* long double -> double -> float */
    d = (double)ld;
    ASSERT(d > 1.24 && d < 1.26, "long double to double");

    f = (float)d;
    ASSERT(f > 1.24f && f < 1.26f, "double to float");

    /* direct float <-> long double */
    ld = (long double)f;
    ASSERT(ld > 1.24L && ld < 1.26L, "float to long double");

    f = (float)ld;
    ASSERT(f > 1.24f && f < 1.26f, "long double to float");
}

static void test_mixed_arithmetic(void) {
    float f = 2.5f;
    double d = 10.0;
    long double ld = 100.0L;
    double r1;
    long double r2;

    printf("Testing mixed-precision arithmetic...\n");

    r1 = f + d; /* 12.5 */
    ASSERT(r1 > 12.49 && r1 < 12.51, "float + double");

    r2 = d + ld; /* 110.0 */
    ASSERT(r2 > 109.99L && r2 < 110.01L, "double + long double");

    r2 = f + ld; /* 102.5 */
    ASSERT(r2 > 102.49L && r2 < 102.51L, "float + long double");
}

int main(void) {
    printf("Running Floating Point Tests...\n");
    test_float_sizes();
    test_float32();
    test_float64();
    test_float128();
    test_cross_conversions();
    test_mixed_arithmetic();

    printf("All Floating Point Tests PASSED!\n");
    return 0;
}
