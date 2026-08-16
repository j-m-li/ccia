/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>

int test_if_else(void) {
    int x = 10;
    int res = 0;

    if (x == 10) {
        res = 1;
    } else {
        res = 2;
    }
    if (res != 1) return 1;

    if (x < 5) {
        res = 10;
    } else if (x == 10) {
        res = 20;
    } else {
        res = 30;
    }
    if (res != 20) return 2;

    return 0;
}

int test_while(void) {
    int i = 0;
    int sum = 0;

    while (i < 10) {
        sum += i;
        i++;
    }
    if (sum != 45) return 1;

    i = 0;
    sum = 0;
    while (1) {
        if (i == 5) {
            i++;
            continue;
        }
        if (i > 9) break;
        sum += i;
        i++;
    }
    if (sum != 40) return 2;

    return 0;
}

int test_do_while(void) {
    int i = 0;
    int count = 0;

    do {
        count++;
        i++;
    } while (i < 5);

    if (count != 5) return 1;

    do {
        count++;
    } while (0);

    if (count != 6) return 2;

    return 0;
}

int test_for(void) {
    int i;
    int j;
    int sum = 0;

    for (i = 0; i < 10; i++) {
        sum += i;
    }
    if (sum != 45) return 1;

    sum = 0;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            if (j == 2) continue;
            sum += 1;
        }
    }
    if (sum != 20) return 2;

    return 0;
}

int test_switch(void) {
    int x;
    int val = 0;

    for (x = 1; x <= 4; x++) {
        switch (x) {
            case 1:
                val += 10;
                break;
            case 2:
            case 3:
                val += 20;
                break;
            default:
                val += 100;
                break;
        }
    }
    if (val != (10 + 20 + 20 + 100)) return 1;

    return 0;
}

int test_goto(void) {
    int count = 0;

start:
    count++;
    if (count < 5) {
        goto start;
    }

    if (count != 5) return 1;

    goto skip;
    return 2;

skip:
    return 0;
}

int main(void) {
    if (test_if_else() != 0) return 1;
    if (test_while() != 0) return 2;
    if (test_do_while() != 0) return 3;
    if (test_for() != 0) return 4;
    if (test_switch() != 0) return 5;
    if (test_goto() != 0) return 6;

    printf("PASS: test_control\n");
    return 0;
}
