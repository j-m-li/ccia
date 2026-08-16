/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>

typedef struct Point {
    int x;
    int y;
} Point;

typedef struct Rect {
    Point top_left;
    Point bottom_right;
} Rect;

typedef union Data {
    int i;
    char bytes[4];
} Data;

typedef enum Color {
    COLOR_RED = 1,
    COLOR_GREEN = 10,
    COLOR_BLUE = 20
} Color;

int test_struct_basics(void) {
    Point p1;
    Point p2;
    Point *pp;

    p1.x = 10;
    p1.y = 20;

    if (p1.x != 10) return 1;
    if (p1.y != 20) return 2;

    pp = &p1;
    if (pp->x != 10) return 3;
    if (pp->y != 20) return 4;

    pp->x = 30;
    pp->y = 40;
    if (p1.x != 30) return 5;
    if (p1.y != 40) return 6;

    /* Struct copy */
    p2 = p1;
    if (p2.x != 30) return 7;
    if (p2.y != 40) return 8;

    return 0;
}

int test_nested_structs(void) {
    Rect r;
    r.top_left.x = 1;
    r.top_left.y = 2;
    r.bottom_right.x = 10;
    r.bottom_right.y = 20;

    if (r.top_left.x != 1) return 1;
    if (r.top_left.y != 2) return 2;
    if (r.bottom_right.x != 10) return 3;
    if (r.bottom_right.y != 20) return 4;

    return 0;
}

int test_unions(void) {
    Data d;
    d.i = 0x12345678;

    if (sizeof(Data) != 4) return 1;
    if (d.bytes[0] != 0x78) return 2; /* Little-endian on x86_64 */
    if (d.bytes[1] != 0x56) return 3;

    return 0;
}

int test_enums(void) {
    Color c = COLOR_GREEN;
    if (COLOR_RED != 1) return 1;
    if (COLOR_GREEN != 10) return 2;
    if (COLOR_BLUE != 20) return 3;
    if (c != 10) return 4;

    return 0;
}

int main(void) {
    if (test_struct_basics() != 0) return 1;
    if (test_nested_structs() != 0) return 2;
    if (test_unions() != 0) return 3;
    if (test_enums() != 0) return 4;

    printf("PASS: test_structs\n");
    return 0;
}
