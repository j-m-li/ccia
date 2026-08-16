/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

static Scope *current_scope = NULL;
static Scope *global_scope = NULL;

Scope *scope_new(Scope *parent) {
    Scope *s = (Scope *)c90_malloc(sizeof(Scope));
    s->vars = map_new();
    s->tags = map_new();
    s->parent = parent;
    s->level = parent ? parent->level + 1 : 0;
    return s;
}

void scope_enter(void) {
    if (!global_scope) {
        global_scope = scope_new(NULL);
        current_scope = global_scope;
    } else {
        current_scope = scope_new(current_scope);
    }
}

void scope_exit(void) {
    if (current_scope && current_scope->parent) {
        current_scope = current_scope->parent;
    }
}

Symbol *symbol_new(SymbolKind kind, const char *name, Type *type) {
    Symbol *sym = (Symbol *)c90_malloc(sizeof(Symbol));
    sym->kind = kind;
    sym->name = name ? c90_strdup(name) : NULL;
    sym->type = type;
    sym->storage = STORAGE_AUTO;
    sym->is_global = (current_scope && current_scope->level == 0);
    sym->is_defined = 0;
    sym->stack_offset = 0;
    sym->asm_label = NULL;
    sym->enum_value = 0;
    sym->init = NULL;
    sym->scope_level = current_scope ? current_scope->level : 0;
    return sym;
}

Symbol *scope_lookup(const char *name) {
    Scope *s = current_scope;
    while (s) {
        Symbol *sym = (Symbol *)map_get(s->vars, name);
        if (sym) return sym;
        s = s->parent;
    }
    return NULL;
}

Symbol *scope_lookup_current(const char *name) {
    if (!current_scope) return NULL;
    return (Symbol *)map_get(current_scope->vars, name);
}

Type *scope_lookup_tag(const char *tag) {
    Scope *s = current_scope;
    while (s) {
        Type *t = (Type *)map_get(s->tags, tag);
        if (t) return t;
        s = s->parent;
    }
    return NULL;
}

Type *scope_lookup_tag_current(const char *tag) {
    if (!current_scope) return NULL;
    return (Type *)map_get(current_scope->tags, tag);
}

void scope_add_symbol(Symbol *sym) {
    if (!current_scope || !sym || !sym->name) return;
    map_put(current_scope->vars, sym->name, sym);
}

void scope_add_tag(const char *tag, Type *type) {
    if (!current_scope || !tag || !type) return;
    map_put(current_scope->tags, tag, type);
}
