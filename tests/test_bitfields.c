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

/* 1. Basic packed unsigned bitfields */
struct PackedBytes {
    unsigned char a : 3;
    unsigned char b : 5;
};

struct PackedInts {
    unsigned int a : 3;
    unsigned int b : 5;
    unsigned int c : 24;
};

/* 2. Zero-width bitfield forcing alignment */
struct ZeroWidth {
    unsigned int a : 3;
    unsigned int : 0;
    unsigned int b : 5;
};

/* 3. Unnamed padding bitfield */
struct UnnamedPadding {
    unsigned int a : 4;
    unsigned int : 4;
    unsigned int b : 8;
};

/* 4. Signed bitfields */
struct SignedBits {
    signed int sign_1 : 1;
    signed int sign_3 : 3;
    signed int sign_6 : 6;
    unsigned int u_8 : 8;
};

/* 5. Global static initializer test */
struct GlobalInit {
    unsigned int f1 : 4;
    unsigned int f2 : 6;
    unsigned int f3 : 12;
    int f4 : 10;
};

static struct GlobalInit g_bf = { 0xA, 0x25, 0xABC, -15 };

static void test_sizes_and_layout(void) {
    ASSERT(sizeof(struct PackedBytes) == 1, "sizeof PackedBytes == 1");
    ASSERT(sizeof(struct PackedInts) == 4, "sizeof PackedInts == 4");
    ASSERT(sizeof(struct ZeroWidth) == 8, "sizeof ZeroWidth == 8");
    ASSERT(sizeof(struct UnnamedPadding) == 4, "sizeof UnnamedPadding == 4");
}

static void test_unsigned_operations(void) {
    struct PackedInts p;
    p.a = 5;
    p.b = 19;
    p.c = 0x5A5A5A;

    ASSERT(p.a == 5, "read p.a");
    ASSERT(p.b == 19, "read p.b");
    ASSERT(p.c == 0x5A5A5A, "read p.c");

    /* Verify modifying one doesn't clobber others */
    p.a = 7;
    ASSERT(p.a == 7, "p.a updated");
    ASSERT(p.b == 19, "p.b preserved");
    ASSERT(p.c == 0x5A5A5A, "p.c preserved");

    p.b = 0;
    ASSERT(p.a == 7, "p.a preserved after p.b=0");
    ASSERT(p.b == 0, "p.b updated to 0");
    ASSERT(p.c == 0x5A5A5A, "p.c preserved after p.b=0");

    /* Increment and Decrement */
    p.a++;
    ASSERT(p.a == 0, "p.a overflow wraps modulo 8");
    p.a--;
    ASSERT(p.a == 7, "p.a underflow wraps to 7");

    /* Compound assignments */
    p.b = 2;
    p.b += 10;
    ASSERT(p.b == 12, "p.b += 10");
    p.b -= 5;
    ASSERT(p.b == 7, "p.b -= 5");
    p.b *= 3;
    ASSERT(p.b == 21, "p.b *= 3");
    p.b &= 0xF;
    ASSERT(p.b == 5, "p.b &= 0xF");
    p.b |= 0x10;
    ASSERT(p.b == 21, "p.b |= 0x10");
    p.b ^= 0x11;
    ASSERT(p.b == 4, "p.b ^= 0x11");
    p.b <<= 1;
    ASSERT(p.b == 8, "p.b <<= 1");
    p.b >>= 2;
    ASSERT(p.b == 2, "p.b >>= 2");
}

static void test_signed_bitfields(void) {
    struct SignedBits s;

    /* 1-bit signed bitfield can be 0 or -1 */
    s.sign_1 = 0;
    ASSERT(s.sign_1 == 0, "1-bit signed 0");
    s.sign_1 = 1;
    ASSERT(s.sign_1 == -1, "1-bit signed 1 is -1");

    /* 3-bit signed bitfield range: -4 to 3 */
    s.sign_3 = 3;
    ASSERT(s.sign_3 == 3, "3-bit signed 3");
    s.sign_3 = -4;
    ASSERT(s.sign_3 == -4, "3-bit signed -4");
    s.sign_3 = -1;
    ASSERT(s.sign_3 == -1, "3-bit signed -1");
    s.sign_3 = 0;
    ASSERT(s.sign_3 == 0, "3-bit signed 0");

    /* 6-bit signed bitfield range: -32 to 31 */
    s.sign_6 = -25;
    ASSERT(s.sign_6 == -25, "6-bit signed -25");
    ASSERT(s.sign_6 < 0, "6-bit signed < 0");
    ASSERT(s.sign_6 + 30 == 5, "6-bit signed arithmetic");

    s.sign_6 = 25;
    ASSERT(s.sign_6 == 25, "6-bit signed 25");
    ASSERT(s.sign_6 > 0, "6-bit signed > 0");

    /* Mixed signed/unsigned struct */
    s.u_8 = 200;
    ASSERT(s.u_8 == 200, "u_8 unsigned value");
    ASSERT(s.sign_6 == 25, "sign_6 unaffected by u_8");
}

static void test_unnamed_and_zero_width(void) {
    struct ZeroWidth zw;
    zw.a = 6;
    zw.b = 27;
    ASSERT(zw.a == 6, "ZeroWidth a");
    ASSERT(zw.b == 27, "ZeroWidth b");

    struct UnnamedPadding up;
    up.a = 0xF;
    up.b = 0xAA;
    ASSERT(up.a == 0xF, "UnnamedPadding a");
    ASSERT(up.b == 0xAA, "UnnamedPadding b");
}

static void test_pointer_access(void) {
    struct PackedInts p;
    struct PackedInts *ptr = &p;
    ptr->a = 4;
    ptr->b = 15;
    ptr->c = 0x123456;

    ASSERT(ptr->a == 4, "pointer read ptr->a");
    ASSERT(ptr->b == 15, "pointer read ptr->b");
    ASSERT(ptr->c == 0x123456, "pointer read ptr->c");

    ptr->a++;
    ASSERT(ptr->a == 5, "pointer ptr->a++");
    ptr->b += 10;
    ASSERT(ptr->b == 25, "pointer ptr->b += 10");
}

static void test_initializers(void) {
    /* Global initializer */
    ASSERT(g_bf.f1 == 0xA, "global f1 == 0xA");
    ASSERT(g_bf.f2 == 0x25, "global f2 == 0x25");
    ASSERT(g_bf.f3 == 0xABC, "global f3 == 0xABC");
    ASSERT(g_bf.f4 == -15, "global f4 == -15");

    /* Local compound initializer */
    struct GlobalInit loc = { 0x5, 0x1F, 0x777, -100 };
    ASSERT(loc.f1 == 0x5, "local f1 == 0x5");
    ASSERT(loc.f2 == 0x1F, "local f2 == 0x1F");
    ASSERT(loc.f3 == 0x777, "local f3 == 0x777");
    ASSERT(loc.f4 == -100, "local f4 == -100");
}

int main(void) {
    printf("Running Bitfield Tests...\n");

    test_sizes_and_layout();
    test_unsigned_operations();
    test_signed_bitfields();
    test_unnamed_and_zero_width();
    test_pointer_access();
    test_initializers();

    printf("Bitfield Tests Passed: %d / %d\n", passed_tests, total_tests);
    return (passed_tests == total_tests) ? 0 : 1;
}
