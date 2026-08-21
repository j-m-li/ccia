/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

/* ========================================================================= */
/* Memory Allocation and Safe Wrappers                                       */
/* ========================================================================= */

void *c90_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "ccia: out of memory (failed to allocate %lu bytes)\n", (unsigned long)size);
        exit(1);
    }
    return ptr;
}

void *c90_calloc(size_t count, size_t size) {
    void *ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "ccia: out of memory (failed to allocate %lu bytes)\n", (unsigned long)(count * size));
        exit(1);
    }
    return ptr;
}

void *c90_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "ccia: out of memory (failed to reallocate %lu bytes)\n", (unsigned long)size);
        exit(1);
    }
    return new_ptr;
}

char *c90_strdup(const char *s) {
    size_t len;
    char *dup;
    if (!s) return NULL;
    len = strlen(s);
    dup = (char *)c90_malloc(len + 1);
    memcpy(dup, s, len + 1);
    return dup;
}

char *c90_strndup(const char *s, int n) {
    char *dup;
    if (!s) return NULL;
    dup = (char *)c90_malloc(n + 1);
    memcpy(dup, s, n);
    dup[n] = '\0';
    return dup;
}

void c90_error(const char *filename, int line, const char *fmt, ...) {
    va_list ap;
    if (filename) {
        fprintf(stderr, "%s:%d: error: ", filename, line);
    } else {
        fprintf(stderr, "ccia: error: ");
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void c90_warn(const char *filename, int line, const char *fmt, ...) {
    va_list ap;
    if (filename) {
        fprintf(stderr, "%s:%d: warning: ", filename, line);
    } else {
        fprintf(stderr, "ccia: warning: ");
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ========================================================================= */
/* Dynamic Array / Vector                                                    */
/* ========================================================================= */

Vector *vec_new(void) {
    Vector *v = (Vector *)c90_malloc(sizeof(Vector));
    v->size = 0;
    v->capacity = 8;
    v->data = (void **)c90_malloc(sizeof(void *) * v->capacity);
    return v;
}

void vec_push(Vector *v, void *elem) {
    if (v->size >= v->capacity) {
        v->capacity *= 2;
        v->data = (void **)c90_realloc(v->data, sizeof(void *) * v->capacity);
    }
    v->data[v->size++] = elem;
}

void *vec_pop(Vector *v) {
    if (v->size == 0) return NULL;
    return v->data[--v->size];
}

void *vec_get(Vector *v, int index) {
    if (index < 0 || index >= v->size) return NULL;
    return v->data[index];
}

void vec_set(Vector *v, int index, void *elem) {
    if (index < 0 || index >= v->size) return;
    v->data[index] = elem;
}

void vec_free(Vector *v) {
    if (!v) return;
    if (v->data) free(v->data);
    free(v);
}

/* ========================================================================= */
/* Hash Map                                                                  */
/* ========================================================================= */

static unsigned long hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++) != 0) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

Map *map_new(void) {
    Map *m = (Map *)c90_malloc(sizeof(Map));
    m->bucket_count = 64;
    m->size = 0;
    m->buckets = (MapEntry **)c90_calloc(m->bucket_count, sizeof(MapEntry *));
    return m;
}

static void map_resize(Map *m) {
    int new_cap = m->bucket_count * 2;
    MapEntry **new_buckets = (MapEntry **)c90_calloc(new_cap, sizeof(MapEntry *));
    int i;

    for (i = 0; i < m->bucket_count; i++) {
        MapEntry *entry = m->buckets[i];
        while (entry) {
            MapEntry *next = entry->next;
            unsigned long h = hash_string(entry->key) % new_cap;
            entry->next = new_buckets[h];
            new_buckets[h] = entry;
            entry = next;
        }
    }
    free(m->buckets);
    m->buckets = new_buckets;
    m->bucket_count = new_cap;
}

void map_put(Map *m, const char *key, void *val) {
    unsigned long h;
    MapEntry *entry;

    if ((m->size + 1) * 4 > m->bucket_count * 3) {
        map_resize(m);
    }

    h = hash_string(key) % m->bucket_count;
    entry = m->buckets[h];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->val = val;
            return;
        }
        entry = entry->next;
    }

    entry = (MapEntry *)c90_malloc(sizeof(MapEntry));
    entry->key = c90_strdup(key);
    entry->val = val;
    entry->next = m->buckets[h];
    m->buckets[h] = entry;
    m->size++;
}

void *map_get(Map *m, const char *key) {
    unsigned long h;
    MapEntry *entry;
    if (!m || !key) return NULL;
    h = hash_string(key) % m->bucket_count;
    entry = m->buckets[h];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->val;
        }
        entry = entry->next;
    }
    return NULL;
}

int map_has(Map *m, const char *key) {
    return map_get(m, key) != NULL;
}

void map_free(Map *m) {
    int i;
    if (!m) return;
    for (i = 0; i < m->bucket_count; i++) {
        MapEntry *entry = m->buckets[i];
        while (entry) {
            MapEntry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(m->buckets);
    free(m);
}

/* ========================================================================= */
/* Dynamic String Buffer                                                     */
/* ========================================================================= */

StrBuf *strbuf_new(void) {
    StrBuf *sb = (StrBuf *)c90_malloc(sizeof(StrBuf));
    sb->capacity = 32;
    sb->length = 0;
    sb->data = (char *)c90_malloc(sb->capacity);
    sb->data[0] = '\0';
    return sb;
}

void strbuf_append_char(StrBuf *sb, char c) {
    if (sb->length + 2 > sb->capacity) {
        sb->capacity *= 2;
        sb->data = (char *)c90_realloc(sb->data, sb->capacity);
    }
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

void strbuf_append_str(StrBuf *sb, const char *s) {
    int len;
    if (!s) return;
    len = (int)strlen(s);
    if (sb->length + len + 1 > sb->capacity) {
        while (sb->length + len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = (char *)c90_realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->length, s, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

void strbuf_append_buf(StrBuf *sb, const char *s, int len) {
    if (!s || len <= 0) return;
    if (sb->length + len + 1 > sb->capacity) {
        while (sb->length + len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = (char *)c90_realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->length, s, len);
    sb->length += len;
    sb->data[sb->length] = '\0';
}

char *strbuf_to_string(StrBuf *sb) {
    char *res;
    if (!sb) return NULL;
    res = c90_strdup(sb->data);
    return res;
}

void strbuf_free(StrBuf *sb) {
    if (!sb) return;
    if (sb->data) free(sb->data);
    free(sb);
}
