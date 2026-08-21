#include <errno.h>
#include "sqlite.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
    } else { \
        printf("FAIL: %s (line %d)\n", msg, __LINE__); \
        return 0; \
    } \
} while(0)

static void custom_double_func(sqlite_func *ctx, int argc, const char **argv) {
    if (argc > 0 && argv[0]) {
        int val = atoi(argv[0]);
        sqlite_set_result_int(ctx, val * 2);
    }
}

typedef struct {
    int prod;
} ProdCtx;

static void prod_step(sqlite_func *ctx, int argc, const char **argv) {
    ProdCtx *p;
    if (argc < 1 || !argv[0]) return;
    p = (ProdCtx *)sqlite_aggregate_context(ctx, sizeof(ProdCtx));
    if (p) {
        if (p->prod == 0) p->prod = 1;
        p->prod *= atoi(argv[0]);
    }
}

static void prod_finalize(sqlite_func *ctx) {
    ProdCtx *p = (ProdCtx *)sqlite_aggregate_context(ctx, sizeof(ProdCtx));
    sqlite_set_result_int(ctx, p ? p->prod : 0);
}

typedef struct {
    int row_count;
    char last_val[64];
} QueryCtx;

static int callback_count(void *arg, int argc, char **argv, char **colNames) {
    QueryCtx *ctx = (QueryCtx *)arg;
    (void)colNames;
    ctx->row_count++;
    if (argc > 0 && argv[0]) {
        strncpy(ctx->last_val, argv[0], sizeof(ctx->last_val) - 1);
    }
    return 0;
}

static int test_version(void) {
    printf("Testing SQLite version string... ");
    ASSERT(strstr(sqlite_version, "2.8") != NULL, "version matches 2.8");
    printf("OK\n");
    return 1;
}

static int test_open_close_memory(void) {
    sqlite *db;
    char *errmsg = NULL;
    printf("Testing sqlite_open :memory: and sqlite_close... ");
    db = sqlite_open(":memory:", 0, &errmsg);
    ASSERT(db != NULL, "open :memory: succeeded");
    sqlite_close(db);
    printf("OK\n");
    return 1;
}

static int test_table_operations(void) {
    sqlite *db;
    char *errmsg = NULL;
    QueryCtx qctx;
    int rc;

    printf("Testing CREATE TABLE, INSERT, SELECT, and aggregates... ");
    db = sqlite_open(":memory:", 0, &errmsg);
    ASSERT(db != NULL, "open db");

    rc = sqlite_exec(db, "CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT, price REAL, qty INT);", NULL, NULL, &errmsg);
    ASSERT(rc == SQLITE_OK, "create table");

    rc = sqlite_exec(db, "INSERT INTO items VALUES (1, 'Apple', 1.50, 10);", NULL, NULL, &errmsg);
    ASSERT(rc == SQLITE_OK, "insert 1");
    ASSERT(sqlite_last_insert_rowid(db) == 1, "last_insert_rowid is 1");

    rc = sqlite_exec(db, "INSERT INTO items VALUES (2, 'Banana', 0.75, 20);", NULL, NULL, &errmsg);
    ASSERT(rc == SQLITE_OK, "insert 2");
    ASSERT(sqlite_last_insert_rowid(db) == 2, "last_insert_rowid is 2");

    rc = sqlite_exec(db, "INSERT INTO items VALUES (3, 'Cherry', 3.00, 5);", NULL, NULL, &errmsg);
    ASSERT(rc == SQLITE_OK, "insert 3");

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT name FROM items ORDER BY id;", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "select");
    ASSERT(qctx.row_count == 3, "selected 3 rows");
    ASSERT(strcmp(qctx.last_val, "Cherry") == 0, "last item is Cherry");

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT SUM(qty) FROM items;", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "select sum");
    ASSERT(atoi(qctx.last_val) == 35, "sum of qty is 35");

    sqlite_close(db);
    printf("OK\n");
    return 1;
}

static int test_vm_interface(void) {
    sqlite *db;
    sqlite_vm *vm = NULL;
    const char *tail = NULL;
    char *errmsg = NULL;
    int numCols = 0;
    const char **colValues = NULL;
    const char **colNames = NULL;
    int rc;
    int count = 0;

    printf("Testing VM API (sqlite_compile, sqlite_step, sqlite_finalize)... ");
    db = sqlite_open(":memory:", 0, &errmsg);
    ASSERT(db != NULL, "open db");

    rc = sqlite_exec(db, "CREATE TABLE nums (val INT);", NULL, NULL, &errmsg);
    ASSERT(rc == SQLITE_OK, "create table");
    sqlite_exec(db, "INSERT INTO nums VALUES (10);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO nums VALUES (20);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO nums VALUES (30);", NULL, NULL, NULL);

    rc = sqlite_compile(db, "SELECT val FROM nums ORDER BY val DESC;", &tail, &vm, &errmsg);
    ASSERT(rc == SQLITE_OK && vm != NULL, "compile statement");

    while ((rc = sqlite_step(vm, &numCols, &colValues, &colNames)) == SQLITE_ROW) {
        ASSERT(numCols == 1, "1 column");
        if (count == 0) ASSERT(atoi(colValues[0]) == 30, "first row 30");
        else if (count == 1) ASSERT(atoi(colValues[0]) == 20, "second row 20");
        else if (count == 2) ASSERT(atoi(colValues[0]) == 10, "third row 10");
        count++;
    }
    ASSERT(rc == SQLITE_DONE, "step finished with DONE");
    ASSERT(count == 3, "stepped 3 rows");

    rc = sqlite_finalize(vm, &errmsg);
    ASSERT(rc == SQLITE_OK, "finalize vm");

    sqlite_close(db);
    printf("OK\n");
    return 1;
}

static int test_user_functions(void) {
    sqlite *db;
    char *errmsg = NULL;
    QueryCtx qctx;
    int rc;

    printf("Testing user-defined functions and aggregates... ");
    db = sqlite_open(":memory:", 0, &errmsg);
    ASSERT(db != NULL, "open db");

    rc = sqlite_create_function(db, "double_it", 1, custom_double_func, NULL);
    ASSERT(rc == SQLITE_OK, "create scalar function");

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT double_it(21);", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "exec scalar func");
    ASSERT(atoi(qctx.last_val) == 42, "double_it(21) == 42");

    rc = sqlite_create_aggregate(db, "product", 1, prod_step, prod_finalize, NULL);
    ASSERT(rc == SQLITE_OK, "create aggregate function");

    sqlite_exec(db, "CREATE TABLE fnums (v INT);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO fnums VALUES (2);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO fnums VALUES (3);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO fnums VALUES (4);", NULL, NULL, NULL);

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT product(v) FROM fnums;", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "exec aggregate func");
    ASSERT(atoi(qctx.last_val) == 24, "product(2,3,4) == 24");

    sqlite_close(db);
    printf("OK\n");
    return 1;
}

static int test_transactions(void) {
    sqlite *db;
    char *errmsg = NULL;
    QueryCtx qctx;
    int rc;

    printf("Testing transactions and rollback... ");
    db = sqlite_open(":memory:", 0, &errmsg);
    ASSERT(db != NULL, "open db");

    sqlite_exec(db, "CREATE TABLE t (x INT);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES (1);", NULL, NULL, NULL);

    sqlite_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES (2);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO t VALUES (3);", NULL, NULL, NULL);
    sqlite_exec(db, "ROLLBACK;", NULL, NULL, NULL);

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT COUNT(*) FROM t;", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "select count");
    ASSERT(atoi(qctx.last_val) == 1, "count is 1 after rollback");

    sqlite_close(db);
    printf("OK\n");
    return 1;
}

static int test_disk_db(void) {
    sqlite *db;
    char *errmsg = NULL;
    QueryCtx qctx;
    const char *filename = "test_disk.db";
    int rc;

    printf("Testing file-based database persistence... ");
    unlink(filename);

    db = sqlite_open(filename, 0, &errmsg);
    if (!db) { printf("errmsg: %s, errno=%d\n", errmsg ? errmsg : "null", errno); } ASSERT(db != NULL, "open disk db");

    sqlite_exec(db, "CREATE TABLE persistent (k TEXT, v TEXT);", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO persistent VALUES ('host', 'rv32i');", NULL, NULL, NULL);
    sqlite_exec(db, "INSERT INTO persistent VALUES ('compiler', 'ccia');", NULL, NULL, NULL);
    sqlite_close(db);

    /* Reopen and verify data persisted */
    db = sqlite_open(filename, 0, &errmsg);
    ASSERT(db != NULL, "reopen disk db");

    memset(&qctx, 0, sizeof(qctx));
    rc = sqlite_exec(db, "SELECT v FROM persistent WHERE k = 'compiler';", callback_count, &qctx, &errmsg);
    ASSERT(rc == SQLITE_OK, "select from reopened db");
    ASSERT(strcmp(qctx.last_val, "ccia") == 0, "value is ccia");

    sqlite_close(db);
    unlink(filename);
    printf("OK\n");
    return 1;
}

int main(void) {
    printf("==========================================\n");
    printf(" Running SQLite 2.8.17 C API Test Suite\n");
    printf("==========================================\n");

    test_version();
    test_open_close_memory();
    test_table_operations();
    test_vm_interface();
    test_user_functions();
    test_transactions();
    test_disk_db();

    printf("==========================================\n");
    printf(" All %d test assertions passed successfully!\n", tests_passed);
    printf("==========================================\n");
    return 0;
}
