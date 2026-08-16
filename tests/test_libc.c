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

int main(void) {
    if (test_memory() != 0) return 1;
    if (test_strings() != 0) return 2;
    if (test_linked_list() != 0) return 3;

    printf("PASS: test_libc\n");
    return 0;
}
