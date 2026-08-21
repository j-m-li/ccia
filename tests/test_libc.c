/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int compare_ints(const void *a, const void *b) {
    int arg1 = *(const int *)a;
    int arg2 = *(const int *)b;
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}

int test_memory(void) {
    int *arr = (int *)malloc(10 * sizeof(int));
    int i;

    if (!arr) return 1;

    for (i = 0; i < 10; i++) {
        arr[i] = (9 - i) * 3;
    }

    qsort(arr, 10, sizeof(int), compare_ints);

    for (i = 0; i < 10; i++) {
        if (arr[i] != i * 3) {
            free(arr);
            return 2;
        }
    }

    free(arr);
    return 0;
}

int test_strings(void) {
    char buf[128];
    memset(buf, 0, sizeof(buf));

    strcpy(buf, "Hello");
    strcat(buf, ", ");
    strcat(buf, "World!");

    if (strlen(buf) != 13) return 1;
    if (strcmp(buf, "Hello, World!") != 0) return 2;

    sprintf(buf, "%d + %d = %d", 10, 20, 30);
    if (strcmp(buf, "10 + 20 = 30") != 0) return 3;

    return 0;
}

typedef struct Node {
    int val;
    struct Node *next;
} Node;

int test_linked_list(void) {
    Node *head = NULL;
    Node *cur;
    int i;
    int sum = 0;

    for (i = 1; i <= 5; i++) {
        Node *n = (Node *)malloc(sizeof(Node));
        n->val = i * 10;
        n->next = head;
        head = n;
    }

    cur = head;
    while (cur) {
        sum += cur->val;
        cur = cur->next;
    }

    if (sum != (10 + 20 + 30 + 40 + 50)) return 1;

    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }

    return 0;
}

int test_system(void) {
    int r;
    r = system(NULL);
    if (r == 0) return 1;

    r = system("true");
    if (r != 0) return 2;

    r = system("exit 0");
    if (r != 0) return 3;

    return 0;
}

int test_malloc_free_realloc(void) {
    void *p1 = malloc(128);
    void *p2 = malloc(256);
    void *p3 = malloc(512);
    void *p2_reuse;
    void *bc;
    char *rp;
    int *carr;
    int i;

    if (!p1 || !p2 || !p3) return 1;
    memset(p1, 0xAA, 128);
    memset(p2, 0xBB, 256);
    memset(p3, 0xCC, 512);

    free(p2);
    p2_reuse = malloc(100);
    if (!p2_reuse) return 2;

    free(p1);
    free(p2_reuse);
    free(p3);

    /* Coalesce test */
    p1 = malloc(100);
    p2 = malloc(100);
    p3 = malloc(100);
    free(p2);
    free(p3);
    bc = malloc(220);
    if (!bc) return 3;
    free(p1);
    free(bc);

    /* Realloc test */
    rp = (char *)malloc(50);
    if (!rp) return 4;
    for (i = 0; i < 50; i++) rp[i] = (char)('a' + (i % 26));
    rp = (char *)realloc(rp, 200);
    if (!rp) return 5;
    for (i = 0; i < 50; i++) {
        if (rp[i] != (char)('a' + (i % 26))) {
            free(rp);
            return 6;
        }
    }
    free(rp);

    /* Calloc test */
    carr = (int *)calloc(50, sizeof(int));
    if (!carr) return 7;
    for (i = 0; i < 50; i++) {
        if (carr[i] != 0) {
            free(carr);
            return 8;
        }
    }
    free(carr);

    return 0;
}

int main(void) {
    if (test_memory() != 0) return 1;
    if (test_strings() != 0) return 2;
    if (test_linked_list() != 0) return 3;
    if (test_system() != 0) return 4;
    if (test_malloc_free_realloc() != 0) return 5;

    printf("PASS: test_libc\n");
    return 0;
}
