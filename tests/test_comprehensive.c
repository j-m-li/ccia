/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Binary Search Tree implementation */
typedef struct BSTNode {
    int key;
    int value;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

BSTNode *bst_insert(BSTNode *root, int key, int value) {
    if (!root) {
        BSTNode *n = (BSTNode *)malloc(sizeof(BSTNode));
        n->key = key;
        n->value = value;
        n->left = NULL;
        n->right = NULL;
        return n;
    }
    if (key < root->key) {
        root->left = bst_insert(root->left, key, value);
    } else if (key > root->key) {
        root->right = bst_insert(root->right, key, value);
    } else {
        root->value = value;
    }
    return root;
}

BSTNode *bst_find(BSTNode *root, int key) {
    if (!root || root->key == key) return root;
    if (key < root->key) return bst_find(root->left, key);
    return bst_find(root->right, key);
}

void bst_free(BSTNode *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

int test_bst(void) {
    BSTNode *root = NULL;
    BSTNode *found;
    int keys[7] = {50, 30, 20, 40, 70, 60, 80};
    int i;

    for (i = 0; i < 7; i++) {
        root = bst_insert(root, keys[i], keys[i] * 100);
    }

    found = bst_find(root, 60);
    if (!found || found->value != 6000) {
        bst_free(root);
        return 1;
    }

    found = bst_find(root, 25);
    if (found != NULL) {
        bst_free(root);
        return 2;
    }

    bst_free(root);
    return 0;
}

/* Sieve of Eratosthenes */
int count_primes(int n) {
    char *is_prime = (char *)malloc(n + 1);
    int i, p;
    int count = 0;

    memset(is_prime, 1, n + 1);
    is_prime[0] = 0;
    is_prime[1] = 0;

    for (p = 2; p * p <= n; p++) {
        if (is_prime[p]) {
            for (i = p * p; i <= n; i += p) {
                is_prime[i] = 0;
            }
        }
    }

    for (p = 2; p <= n; p++) {
        if (is_prime[p]) count++;
    }

    free(is_prime);
    return count;
}

int test_primes(void) {
    /* Primes under 100 is 25 */
    if (count_primes(100) != 25) return 1;
    /* Primes under 1000 is 168 */
    if (count_primes(1000) != 168) return 2;
    return 0;
}

/* Matrix Multiplication */
int test_matrix_mult(void) {
    int A[2][3] = {
        {1, 2, 3},
        {4, 5, 6}
    };
    int B[3][2] = {
        {7, 8},
        {9, 1},
        {2, 3}
    };
    int C[2][2];
    int i, j, k;

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            C[i][j] = 0;
            for (k = 0; k < 3; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }

    /* Row 0: 1*7 + 2*9 + 3*2 = 7 + 18 + 6 = 31 */
    /* Row 0, col 1: 1*8 + 2*1 + 3*3 = 8 + 2 + 9 = 19 */
    /* Row 1, col 0: 4*7 + 5*9 + 6*2 = 28 + 45 + 12 = 85 */
    /* Row 1, col 1: 4*8 + 5*1 + 6*3 = 32 + 5 + 18 = 55 */

    if (C[0][0] != 31) return 1;
    if (C[0][1] != 19) return 2;
    if (C[1][0] != 85) return 3;
    if (C[1][1] != 55) return 4;

    return 0;
}

int main(void) {
    if (test_bst() != 0) return 1;
    if (test_primes() != 0) return 2;
    if (test_matrix_mult() != 0) return 3;

    printf("PASS: test_comprehensive\n");
    return 0;
}
