/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <assert.h>

int main(void) {
    int a;
    int b;
    int c;

    /* Basic arithmetic */
    a = 10;
    b = 20;
    c = a + b * 2;
    if (c != 50) return 1;

    c = (a + b) * 2;
    if (c != 60) return 2;

    c = 100 / 4 - 5;
    if (c != 20) return 3;

    c = 23 % 5;
    if (c != 3) return 4;

    /* Bitwise operators */
    a = 0xFF00;
    b = 0x0F0F;
    if ((a & b) != 0x0F00) return 5;
    if ((a | b) != 0xFF0F) return 6;
    if ((a ^ b) != 0xF00F) return 7;
    if ((~0 & 0xFF) != 0xFF) return 8;

    /* Shift operators */
    a = 1;
    if ((a << 4) != 16) return 9;
    if ((16 >> 2) != 4) return 10;

    /* Relational and Equality */
    if (!(10 < 20)) return 11;
    if (!(20 > 10)) return 12;
    if (!(10 <= 10)) return 13;
    if (!(10 >= 10)) return 14;
    if (10 == 20) return 15;
    if (!(10 != 20)) return 16;

    /* Logical operators and short-circuit */
    a = 0;
    b = 1;
    if (!(b || (a = 42))) return 17;
    if (a != 0) return 18; /* short-circuit failed */

    a = 0;
    if (a && (b = 42)) return 19;
    if (b != 1) return 20; /* short-circuit failed */

    /* Increment and Decrement */
    a = 5;
    b = ++a;
    if (a != 6 || b != 6) return 21;

    b = a++;
    if (a != 7 || b != 6) return 22;

    b = --a;
    if (a != 6 || b != 6) return 23;

    b = a--;
    if (a != 5 || b != 6) return 24;

    /* Compound assignment */
    a = 10;
    a += 5;
    if (a != 15) return 25;
    a -= 3;
    if (a != 12) return 26;
    a *= 2;
    if (a != 24) return 27;
    a /= 4;
    if (a != 6) return 28;
    a %= 4;
    if (a != 2) return 29;
    a <<= 3;
    if (a != 16) return 30;
    a >>= 2;
    if (a != 4) return 31;
    a &= 6;
    if (a != 4) return 32;
    a |= 3;
    if (a != 7) return 33;
    a ^= 5;
    if (a != 2) return 34;

    /* Ternary conditional */
    a = 10;
    b = (a > 5) ? 100 : 200;
    if (b != 100) return 35;
    b = (a < 5) ? 100 : 200;
    if (b != 200) return 36;

    /* Comma operator */
    a = (1, 2, 3);
    if (a != 3) return 37;

    return 0;
}
