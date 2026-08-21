/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

Type *type_void = NULL;
Type *type_char = NULL;
Type *type_uchar = NULL;
Type *type_short = NULL;
Type *type_ushort = NULL;
Type *type_int = NULL;
Type *type_uint = NULL;
Type *type_long = NULL;
Type *type_ulong = NULL;
Type *type_float = NULL;
Type *type_double = NULL;
Type *type_ldouble = NULL;

void type_init(void) {
    type_void = type_new(TYPE_VOID, 1, 1);
    type_char = type_new(TYPE_CHAR, 1, 1);
    type_uchar = type_new(TYPE_CHAR, 1, 1);
    type_uchar->is_unsigned = 1;
    
    type_short = type_new(TYPE_SHORT, 2, 2);
    type_ushort = type_new(TYPE_SHORT, 2, 2);
    type_ushort->is_unsigned = 1;

    type_int = type_new(TYPE_INT, 4, 4);
    type_uint = type_new(TYPE_INT, 4, 4);
    type_uint->is_unsigned = 1;

#if defined(TARGET_I386) || defined(TARGET_RISCV32) || defined(TARGET_32BIT)
    type_long = type_new(TYPE_LONG, 4, 4);
    type_ulong = type_new(TYPE_LONG, 4, 4);
    type_ulong->is_unsigned = 1;

    type_float = type_new(TYPE_FLOAT, 4, 4);
    type_double = type_new(TYPE_DOUBLE, 8, 4);
    type_ldouble = type_new(TYPE_LDOUBLE, 16, 4);
#else
    type_long = type_new(TYPE_LONG, 8, 8);
    type_ulong = type_new(TYPE_LONG, 8, 8);
    type_ulong->is_unsigned = 1;

    type_float = type_new(TYPE_FLOAT, 4, 4);
    type_double = type_new(TYPE_DOUBLE, 8, 8);
    type_ldouble = type_new(TYPE_LDOUBLE, 16, 16);
#endif
}

Type *type_new(TypeKind kind, int size, int align) {
    Type *t = (Type *)c90_malloc(sizeof(Type));
    t->kind = kind;
    t->size = size;
    t->align = align;
    t->is_unsigned = 0;
    t->is_const = 0;
    t->is_volatile = 0;
    t->base = NULL;
    t->array_len = 0;
    t->tag = NULL;
    t->members = NULL;
    t->is_complete = 1;
    t->params = NULL;
    t->is_varargs = 0;
    return t;
}

Type *type_pointer_to(Type *base) {
#if defined(TARGET_I386) || defined(TARGET_RISCV32) || defined(TARGET_32BIT)
    Type *t = type_new(TYPE_PTR, 4, 4);
#else
    Type *t = type_new(TYPE_PTR, 8, 8);
#endif
    t->base = base;
    return t;
}

Type *type_array_of(Type *base, int len) {
    Type *t = type_new(TYPE_ARRAY, len >= 0 ? base->size * len : 0, base->align);
    t->base = base;
    t->array_len = len;
    return t;
}

Type *type_func_new(Type *ret_type, Vector *params, int is_varargs) {
    Type *t = type_new(TYPE_FUNC, 1, 1);
    t->base = ret_type;
    t->params = params;
    t->is_varargs = is_varargs;
    return t;
}

Type *type_struct_new(const char *tag, int is_union) {
    Type *t = type_new(is_union ? TYPE_UNION : TYPE_STRUCT, 0, 1);
    t->tag = tag ? c90_strdup(tag) : NULL;
    t->members = NULL;
    t->is_complete = 0;
    return t;
}

Type *type_enum_new(const char *tag) {
    Type *t = type_new(TYPE_ENUM, 4, 4);
    t->tag = tag ? c90_strdup(tag) : NULL;
    return t;
}

Type *type_copy(Type *src) {
    Type *t;
    if (!src) return NULL;
    t = (Type *)c90_malloc(sizeof(Type));
    *t = *src;
    return t;
}

int type_is_integer(Type *t) {
    if (!t) return 0;
    return t->kind == TYPE_CHAR || t->kind == TYPE_SHORT ||
           t->kind == TYPE_INT || t->kind == TYPE_LONG ||
           t->kind == TYPE_ENUM;
}

int type_is_floating(Type *t) {
    if (!t) return 0;
    return t->kind == TYPE_FLOAT || t->kind == TYPE_DOUBLE || t->kind == TYPE_LDOUBLE;
}

int type_is_arithmetic(Type *t) {
    return type_is_integer(t) || type_is_floating(t);
}

int type_is_scalar(Type *t) {
    return type_is_arithmetic(t) || type_is_pointer(t);
}

int type_is_pointer(Type *t) {
    if (!t) return 0;
    return t->kind == TYPE_PTR || t->kind == TYPE_ARRAY;
}

int type_is_void_ptr(Type *t) {
    if (!t) return 0;
    return t->kind == TYPE_PTR && t->base && t->base->kind == TYPE_VOID;
}

Type *type_decay(Type *t) {
    if (!t) return NULL;
    if (t->kind == TYPE_ARRAY) {
        return type_pointer_to(t->base);
    }
    if (t->kind == TYPE_FUNC) {
        return type_pointer_to(t);
    }
    return t;
}

int type_equal(Type *a, Type *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->is_unsigned != b->is_unsigned) return 0;
    if (a->kind == TYPE_PTR || a->kind == TYPE_ARRAY) {
        return type_equal(a->base, b->base);
    }
    if (a->kind == TYPE_STRUCT || a->kind == TYPE_UNION) {
        if (a->tag && b->tag) {
            return strcmp(a->tag, b->tag) == 0;
        }
        return a == b;
    }
    return 1;
}

int type_is_compatible(Type *a, Type *b) {
    if (type_equal(a, b)) return 1;
    if (type_is_pointer(a) && type_is_pointer(b)) {
        if (type_is_void_ptr(a) || type_is_void_ptr(b)) return 1;
        return type_equal(a->base, b->base);
    }
    if (type_is_arithmetic(a) && type_is_arithmetic(b)) {
        return 1;
    }
    return 0;
}

Type *type_max(Type *a, Type *b) {
    if (!a) return b;
    if (!b) return a;
    if (type_is_pointer(a)) return a;
    if (type_is_pointer(b)) return b;
    if (a->kind == TYPE_LDOUBLE || b->kind == TYPE_LDOUBLE) return type_ldouble;
    if (a->kind == TYPE_DOUBLE || b->kind == TYPE_DOUBLE) return type_double;
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT) return type_float;
    if (a->kind == TYPE_LONG || b->kind == TYPE_LONG) {
        if (a->is_unsigned || b->is_unsigned) return type_ulong;
        return type_long;
    }
    if (a->is_unsigned || b->is_unsigned) return type_uint;
    return type_int;
}

Member *type_find_member(Type *struct_type, const char *name) {
    Member *m;
    if (!struct_type || (struct_type->kind != TYPE_STRUCT && struct_type->kind != TYPE_UNION)) {
        return NULL;
    }
    if (!struct_type->members && struct_type->tag) {
        Type *in_scope = scope_lookup_tag(struct_type->tag);
        if (in_scope && in_scope->members) {
            struct_type->members = in_scope->members;
            struct_type->size = in_scope->size;
            struct_type->align = in_scope->align;
            struct_type->is_complete = in_scope->is_complete;
        }
    }
    m = struct_type->members;
    while (m) {
        if (m->name && strcmp(m->name, name) == 0) {
            return m;
        }
        m = m->next;
    }
    return NULL;
}
