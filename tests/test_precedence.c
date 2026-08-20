/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <stdlib.h>

static int total_tests = 0;
static int passed_tests = 0;

#define ASSERT(cond, msg) do { \
    total_tests++; \
    if (cond) { \
        passed_tests++; \
    } else { \
        printf("FAILED: %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

struct Node {
    int val;
    int arr[3];
    struct Node *next;
};

static int g_side_effect = 0;
static int track_side_effect(int val) {
    g_side_effect++;
    return val;
}

/* ========================================================================= */
/* Level 15 (Postfix) vs Level 14 (Unary)                                    */
/* ========================================================================= */
static void test_postfix_vs_unary(void) {
    int arr[3] = { 10, 20, 30 };
    int *p = arr;
    int res;

    /* *p++ -> *(p++) : gets *p then increments pointer */
    res = *p++;
    ASSERT(res == 10, "*p++ returns *p");
    ASSERT(p == &arr[1], "*p++ increments pointer p");

    /* (*p)++ -> post-increments value at pointer */
    res = (*p)++;
    ASSERT(res == 20, "(*p)++ returns old value 20");
    ASSERT(*p == 21, "(*p)++ increments value to 21");

    /* ++*p -> ++(*p) : pre-increments value at pointer */
    res = ++*p;
    ASSERT(res == 22, "++*p increments value to 22");
    ASSERT(*p == 22, "value is 22");

    /* Struct member and array subscript bind tighter than unary */
    {
        struct Node n;
        struct Node *np = &n;
        n.val = 50;
        n.arr[0] = 100;
        n.arr[1] = 200;
        n.next = NULL;

        ASSERT(!n.val == 0, "!n.val == 0");
        ASSERT(~n.val == -51, "~n.val == -51");
        ASSERT(-n.arr[1] == -200, "-n.arr[1] == -200");
        ASSERT(*np->arr == 100, "*np->arr == 100");
        ASSERT(sizeof(n.arr) / sizeof(n.arr[0]) == 3, "sizeof n.arr / sizeof n.arr[0] == 3");
    }
}

/* ========================================================================= */
/* Level 14 (Unary Right-to-Left) and Unary vs Multiplicative (Level 13)     */
/* ========================================================================= */
static void test_unary_and_multiplicative(void) {
    int a = 5, b = 3, c = 2;
    int *p = &a;
    int **pp = &p;

    /* Unary right-to-left associativity */
    ASSERT(!~-a == 0, "!~-a -> ! (~ (-5)) -> !4 -> 0");
    ASSERT(**pp == 5, "**pp -> *(*pp) -> 5");

    /* Unary binds tighter than multiplicative */
    ASSERT(-a * b == -15, "-a * b == (-a) * b");
    ASSERT(a * -b == -15, "a * -b == a * (-b)");
    ASSERT(~a * b == -18, "~a * b == (~5) * 3 == -6 * 3 == -18");
    ASSERT(!0 * b == 3, "!0 * b == 1 * 3 == 3");
    ASSERT((int)3.7 * b == 9, "(int)3.7 * 3 == 3 * 3 == 9");

    /* Multiplicative Left-to-Right Associativity */
    ASSERT(24 / 4 * 2 == 12, "24 / 4 * 2 == (24 / 4) * 2 == 12");
    ASSERT(100 / 5 / 2 == 10, "100 / 5 / 2 == (100 / 5) / 2 == 10");
    ASSERT(20 % 7 % 3 == 6 % 3, "20 % 7 % 3 == (20 % 7) % 3 == 0");
    ASSERT(24 / 4 / 2 * 3 % 5 == ((((24 / 4) / 2) * 3) % 5), "Multiplicative chain");
}

/* ========================================================================= */
/* Level 13 (Multiplicative) vs Level 12 (Additive)                          */
/* ========================================================================= */
static void test_multiplicative_vs_additive(void) {
    int a = 2, b = 3, c = 4, d = 5;

    /* Multiplicative binds tighter than Additive */
    ASSERT(a + b * c == 14, "a + b * c == a + (b * c) == 14");
    ASSERT(a * b + c == 10, "a * b + c == (a * b) + c == 10");
    ASSERT(d - c / a == 3, "d - c / a == d - (c / a) == 3");
    ASSERT(d % b + a * c == 10, "d % b + a * c == (d % b) + (a * c) == 2 + 8 == 10");

    /* Additive Left-to-Right Associativity */
    ASSERT(10 - 5 - 2 == 3, "10 - 5 - 2 == (10 - 5) - 2 == 3 (NOT 10 - 3 = 7)");
    ASSERT(20 - 10 + 5 - 3 == 12, "20 - 10 + 5 - 3 == ((20 - 10) + 5) - 3 == 12");
}

/* ========================================================================= */
/* Level 12 (Additive) vs Level 11 (Shift)                                   */
/* ========================================================================= */
static void test_additive_vs_shift(void) {
    /* Additive binds tighter than Shift! */
    ASSERT((1 << 2 + 1) == 8, "1 << 2 + 1 == 1 << (2 + 1) == 8 (NOT 5)");
    ASSERT((64 >> 4 - 2) == 16, "64 >> 4 - 2 == 64 >> (4 - 2) == 16 (NOT 2)");
    ASSERT((3 + 1 << 4 - 2) == 16, "3 + 1 << 4 - 2 == (3 + 1) << (4 - 2) == 16");

    /* Shift Left-to-Right Associativity */
    ASSERT((1 << 2 << 1) == 8, "1 << 2 << 1 == (1 << 2) << 1 == 8");
    ASSERT((64 >> 2 >> 1) == 8, "64 >> 2 >> 1 == (64 >> 2) >> 1 == 8");
}

/* ========================================================================= */
/* Level 11 (Shift) vs Level 10 (Relational)                                 */
/* ========================================================================= */
static void test_shift_vs_relational(void) {
    /* Shift binds tighter than Relational */
    ASSERT((5 < 1 << 3) == 1, "5 < 1 << 3 == 5 < (1 << 3) == 1");
    ASSERT((10 >= 40 >> 2) == 1, "10 >= 40 >> 2 == 10 >= (40 >> 2) == 1");
    ASSERT((1 << 4 <= 16) == 1, "1 << 4 <= 16 == (1 << 4) <= 16 == 1");

    /* Relational Left-to-Right Associativity */
    /* 3 < 2 < 1 -> (3 < 2) < 1 -> 0 < 1 -> 1 */
    ASSERT((3 < 2 < 1) == 1, "3 < 2 < 1 == (3 < 2) < 1 == 1");
    /* 5 > 4 > 2 -> (5 > 4) > 2 -> 1 > 2 -> 0 */
    ASSERT((5 > 4 > 2) == 0, "5 > 4 > 2 == (5 > 4) > 2 == 0");
    /* 5 <= 5 <= 1 -> (5 <= 5) <= 1 -> 1 <= 1 -> 1 */
    ASSERT((5 <= 5 <= 1) == 1, "5 <= 5 <= 1 == 1");
}

/* ========================================================================= */
/* Level 10 (Relational) vs Level 9 (Equality)                               */
/* ========================================================================= */
static void test_relational_vs_equality(void) {
    /* Relational binds tighter than Equality */
    ASSERT((1 == 2 < 3) == 1, "1 == 2 < 3 == 1 == (2 < 3) == 1 == 1 == 1");
    ASSERT((0 == 5 <= 3) == 1, "0 == 5 <= 3 == 0 == (5 <= 3) == 0 == 0 == 1");
    ASSERT((1 != 10 > 20) == 1, "1 != 10 > 20 == 1 != (10 > 20) == 1 != 0 == 1");

    /* Equality Left-to-Right Associativity */
    ASSERT((2 == 2 == 1) == 1, "2 == 2 == 1 == (2 == 2) == 1 == 1 == 1 == 1");
    ASSERT((1 != 2 != 0) == 1, "1 != 2 != 0 == (1 != 2) != 0 == 1 != 0 == 1");
    ASSERT((3 == 4 == 0) == 1, "3 == 4 == 0 == (3 == 4) == 0 == 0 == 0 == 1");
}

/* ========================================================================= */
/* Level 9 (Equality) vs Level 8 (Bitwise AND)                               */
/* ========================================================================= */
static void test_equality_vs_bitand(void) {
    /* Equality binds tighter than Bitwise AND! */
    ASSERT((7 & 4 == 4) == 1, "7 & 4 == 4 == 7 & (4 == 4) == 7 & 1 == 1 (NOT 4 == 4)");
    ASSERT((6 & 3 == 3) == 0, "6 & 3 == 3 == 6 & (3 == 3) == 6 & 1 == 0");
    ASSERT((7 & 2 != 0) == 1, "7 & 2 != 0 == 7 & (2 != 0) == 7 & 1 == 1");

    /* Bitwise AND Left-to-Right Associativity */
    ASSERT((0xFF & 0x0F & 0x07) == 0x07, "0xFF & 0x0F & 0x07 == ((0xFF & 0x0F) & 0x07)");
}

/* ========================================================================= */
/* Level 8 (Bitwise AND) vs Level 7 (Bitwise XOR)                            */
/* ========================================================================= */
static void test_bitand_vs_bitxor(void) {
    /* Bitwise AND binds tighter than Bitwise XOR */
    ASSERT((7 ^ 3 & 2) == 5, "7 ^ 3 & 2 == 7 ^ (3 & 2) == 7 ^ 2 == 5 (NOT 0)");
    ASSERT((3 & 2 ^ 7) == 5, "3 & 2 ^ 7 == (3 & 2) ^ 7 == 2 ^ 7 == 5");

    /* Bitwise XOR Left-to-Right Associativity */
    ASSERT((15 ^ 7 ^ 3) == 11, "15 ^ 7 ^ 3 == (15 ^ 7) ^ 3 == 8 ^ 3 == 11");
}

/* ========================================================================= */
/* Level 7 (Bitwise XOR) vs Level 6 (Bitwise OR)                             */
/* ========================================================================= */
static void test_bitxor_vs_bitor(void) {
    /* Bitwise XOR binds tighter than Bitwise OR */
    ASSERT((8 | 4 ^ 12) == 8, "8 | 4 ^ 12 == 8 | (4 ^ 12) == 8 | 8 == 8 (NOT 0)");
    ASSERT((4 ^ 12 | 8) == 8, "4 ^ 12 | 8 == (4 ^ 12) | 8 == 8 | 8 == 8");

    /* Bitwise OR Left-to-Right Associativity */
    ASSERT((1 | 2 | 4) == 7, "1 | 2 | 4 == ((1 | 2) | 4) == 7");
}

/* ========================================================================= */
/* Level 6 (Bitwise OR) vs Level 5 (Logical AND)                             */
/* ========================================================================= */
static void test_bitor_vs_logand(void) {
    /* Bitwise OR binds tighter than Logical AND */
    ASSERT((1 && 0 | 2) == 1, "1 && 0 | 2 == 1 && (0 | 2) == 1 && 2 == 1");
    ASSERT((0 | 0 && 1) == 0, "0 | 0 && 1 == (0 | 0) && 1 == 0 && 1 == 0");

    /* Logical AND Left-to-Right Associativity and Short-Circuit */
    g_side_effect = 0;
    ASSERT((1 && 1 && 1) == 1, "1 && 1 && 1 == 1");
    ASSERT((1 && 0 && track_side_effect(1)) == 0, "short circuit logical AND");
    ASSERT(g_side_effect == 0, "right side not evaluated in short-circuit AND");
}

/* ========================================================================= */
/* Level 5 (Logical AND) vs Level 4 (Logical OR)                             */
/* ========================================================================= */
static void test_logand_vs_logor(void) {
    /* Logical AND binds tighter than Logical OR */
    ASSERT((1 || 0 && 0) == 1, "1 || 0 && 0 == 1 || (0 && 0) == 1");
    ASSERT((0 || 1 && 0) == 0, "0 || 1 && 0 == 0 || (1 && 0) == 0");
    ASSERT((0 && 0 || 1) == 1, "0 && 0 || 1 == (0 && 0) || 1 == 1");

    /* Logical OR Left-to-Right Associativity and Short-Circuit */
    g_side_effect = 0;
    ASSERT((0 || 0 || 1) == 1, "0 || 0 || 1 == 1");
    ASSERT((1 || track_side_effect(1)) == 1, "short circuit logical OR");
    ASSERT(g_side_effect == 0, "right side not evaluated in short-circuit OR");
}

/* ========================================================================= */
/* Level 4 (Logical OR) vs Level 3 (Conditional / Ternary Right-to-Left)     */
/* ========================================================================= */
static void test_logor_vs_conditional(void) {
    /* Logical OR binds tighter than Conditional condition */
    ASSERT((0 || 1 ? 100 : 200) == 100, "(0 || 1) ? 100 : 200 == 100");
    ASSERT((0 || 0 ? 100 : 200) == 200, "(0 || 0) ? 100 : 200 == 200");

    /* Conditional Right-to-Left Associativity: a ? b : c ? d : e -> a ? b : (c ? d : e) */
    ASSERT((0 ? 1 : 1 ? 2 : 3) == 2, "0 ? 1 : 1 ? 2 : 3 == 0 ? 1 : (1 ? 2 : 3) == 2");
    ASSERT((0 ? 1 : 0 ? 2 : 3) == 3, "0 ? 1 : 0 ? 2 : 3 == 0 ? 1 : (0 ? 2 : 3) == 3");
    ASSERT((1 ? 0 ? 10 : 20 : 30) == 20, "1 ? 0 ? 10 : 20 : 30 == 1 ? (0 ? 10 : 20) : 30 == 20");
}

/* ========================================================================= */
/* Level 3 (Conditional) vs Level 2 (Assignment Right-to-Left)               */
/* ========================================================================= */
static void test_conditional_vs_assignment(void) {
    int a, b, c;

    /* Assignment Right-to-Left Associativity */
    a = b = c = 42;
    ASSERT(a == 42 && b == 42 && c == 42, "a = b = c = 42");

    a = 1; b = 2; c = 3;
    a += b += c;
    ASSERT(c == 3, "c == 3");
    ASSERT(b == 5, "b == 2 + 3 == 5");
    ASSERT(a == 6, "a == 1 + 5 == 6");

    /* Conditional expression result assigned to variable */
    a = (1 ? 50 : 60);
    ASSERT(a == 50, "a = 1 ? 50 : 60");
    a = (0 ? 50 : 60);
    ASSERT(a == 60, "a = 0 ? 50 : 60");
}

/* ========================================================================= */
/* Level 2 (Assignment) vs Level 1 (Comma Left-to-Right)                     */
/* ========================================================================= */
static void test_assignment_vs_comma(void) {
    int x = 0, y = 0, z = 0;

    /* Assignment binds tighter than comma: x = 1, y = 2, z = 3 */
    x = 1, y = 2, z = 3;
    ASSERT(x == 1 && y == 2 && z == 3, "x = 1, y = 2, z = 3");

    /* Comma operator in parentheses evaluates left-to-right and yields last expr */
    g_side_effect = 0;
    z = (track_side_effect(10), track_side_effect(20), track_side_effect(30));
    ASSERT(z == 30, "comma operator yields last value 30");
    ASSERT(g_side_effect == 3, "comma operator evaluated all expressions in order");

    /* Comma inside assignment RHS */
    x = (y = 5, y * 4);
    ASSERT(y == 5, "y == 5");
    ASSERT(x == 20, "x == 20");
}

/* ========================================================================= */
/* Complex Multi-Precedence Verification                                     */
/* ========================================================================= */
static void test_complex_precedence(void) {
    int a = 2, b = 3, c = 4, d = 5;
    int res;

    /*
     * a + b * c == 14
     * 1 << a + b * c == 1 << 14 == 16384
     * 16384 >> d == 16384 >> 5 == 512
     */
    res = 1 << a + b * c >> d;
    ASSERT(res == 512, "1 << a + b * c >> d == 512");

    /*
     * a < b && c < d || a == b
     * (2 < 3) && (4 < 5) || (2 == 3)
     * 1 && 1 || 0
     * 1 || 0
     * 1
     */
    res = a < b && c < d || a == b;
    ASSERT(res == 1, "a < b && c < d || a == b == 1");

    /*
     * a & b ^ c | d && a < b
     * (((2 & 3) ^ 4) | 5) && (2 < 3)
     * (6 | 5) && 1
     * 7 && 1
     * 1
     */
    res = a & b ^ c | d && a < b;
    ASSERT(res == 1, "a & b ^ c | d && a < b == 1");

    /*
     * Complex ternary with mixed operators:
     * a + b > c ? d << 2 : d >> 1
     * (2 + 3 > 4) ? (5 << 2) : (5 >> 1)
     * (5 > 4) ? 20 : 2
     * 20
     */
    res = a + b > c ? d << 2 : d >> 1;
    ASSERT(res == 20, "a + b > c ? d << 2 : d >> 1 == 20");
}

int main(void) {
    printf("Running Operator Precedence & Associativity Tests...\n");

    test_postfix_vs_unary();
    test_unary_and_multiplicative();
    test_multiplicative_vs_additive();
    test_additive_vs_shift();
    test_shift_vs_relational();
    test_relational_vs_equality();
    test_equality_vs_bitand();
    test_bitand_vs_bitxor();
    test_bitxor_vs_bitor();
    test_bitor_vs_logand();
    test_logand_vs_logor();
    test_logor_vs_conditional();
    test_conditional_vs_assignment();
    test_assignment_vs_comma();
    test_complex_precedence();

    printf("Operator Precedence Tests Passed: %d / %d\n", passed_tests, total_tests);
    return (passed_tests == total_tests) ? 0 : 1;
}
