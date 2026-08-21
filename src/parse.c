/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"
#include "../include/softfloat.h"

/* Forward declarations */
static AstNode *parse_expr(Parser *p);
static AstNode *parse_assignment(Parser *p);
static AstNode *parse_stmt(Parser *p);
static AstNode *parse_compound_stmt(Parser *p);
static Type *parse_decl_specifiers(Parser *p, StorageClass *storage);
static Type *parse_declarator(Parser *p, Type *base, char **out_name);
static Type *parse_type_name(Parser *p);
static Initializer *parse_initializer(Parser *p, Type *type);

/* ========================================================================= */
/* AST Node Constructors                                                     */
/* ========================================================================= */

AstNode *ast_new(AstKind kind, const char *filename, int line) {
    AstNode *n = (AstNode *)c90_malloc(sizeof(AstNode));
    n->kind = kind;
    n->type = NULL;
    n->filename = filename;
    n->line = line;
    return n;
}

AstNode *ast_int_lit(long val, int is_unsigned, Type *type, const char *file, int line) {
    AstNode *n = ast_new(AST_INT_LIT, file, line);
    n->type = type ? type : (is_unsigned ? type_uint : type_int);
    n->u.int_val.val = val;
    n->u.int_val.is_unsigned = is_unsigned;
    return n;
}

AstNode *ast_float_lit(Type *type, const char *file, int line) {
    AstNode *n = ast_new(AST_FLOAT_LIT, file, line);
    n->type = type ? type : type_double;
    n->u.float_val.label = NULL;
    n->u.float_val.u128_words[0] = 0;
    n->u.float_val.u128_words[1] = 0;
    n->u.float_val.u128_words[2] = 0;
    n->u.float_val.u128_words[3] = 0;
    return n;
}

AstNode *ast_str_lit(const char *str, int len, const char *file, int line) {
    AstNode *n = ast_new(AST_STR_LIT, file, line);
    n->type = type_pointer_to(type_char);
    n->u.str_val.str = c90_strdup(str);
    n->u.str_val.len = len;
    n->u.str_val.label = NULL;
    return n;
}

AstNode *ast_char_lit(int c, const char *file, int line) {
    AstNode *n = ast_new(AST_CHAR_LIT, file, line);
    n->type = type_int;
    n->u.int_val.val = c;
    n->u.int_val.is_unsigned = 0;
    return n;
}

AstNode *ast_var(Symbol *sym, const char *file, int line) {
    AstNode *n = ast_new(AST_VAR, file, line);
    n->type = sym ? sym->type : type_int;
    n->u.sym = sym;
    return n;
}

AstNode *ast_binary(AstKind kind, AstNode *lhs, AstNode *rhs, const char *file, int line) {
    AstNode *n = ast_new(kind, file, line);
    n->u.binop.lhs = lhs;
    n->u.binop.rhs = rhs;

    /* Compute expression result type */
    if (kind == AST_EQ || kind == AST_NE || kind == AST_LT ||
        kind == AST_LE || kind == AST_GT || kind == AST_GE ||
        kind == AST_LOGAND || kind == AST_LOGOR) {
        n->type = type_int;
    } else if (kind == AST_ASSIGN) {
        n->type = lhs->type;
    } else if (kind == AST_COMMA) {
        n->type = rhs->type;
    } else if (type_is_pointer(lhs->type) && type_is_integer(rhs->type)) {
        n->type = lhs->type;
    } else if (type_is_pointer(lhs->type) && type_is_pointer(rhs->type) && kind == AST_SUB) {
        n->type = type_long;
    } else {
        n->type = type_max(lhs->type, rhs->type);
    }
    return n;
}

AstNode *ast_unary(AstKind kind, AstNode *operand, const char *file, int line) {
    AstNode *n = ast_new(kind, file, line);
    n->u.unop.operand = operand;

    if (kind == AST_ADDR) {
        n->type = type_pointer_to(operand->type);
    } else if (kind == AST_DEREF) {
        if (operand->type && operand->type->base) {
            n->type = operand->type->base;
        } else {
            n->type = type_int;
        }
    } else if (kind == AST_LOGNOT) {
        n->type = type_int;
    } else {
        n->type = operand->type;
    }
    return n;
}

AstNode *ast_cast(Type *target, AstNode *operand, const char *file, int line) {
    AstNode *n = ast_new(AST_CAST, file, line);
    n->type = target;
    n->u.cast.target_type = target;
    n->u.cast.operand = operand;
    return n;
}

AstNode *ast_call(AstNode *func, Vector *args, const char *file, int line) {
    AstNode *n = ast_new(AST_CALL, file, line);
    n->u.call.func = func;
    n->u.call.args = args;
    if (func->type && func->type->kind == TYPE_FUNC) {
        n->type = func->type->base;
    } else if (func->type && func->type->kind == TYPE_PTR && func->type->base && func->type->base->kind == TYPE_FUNC) {
        n->type = func->type->base->base;
    } else {
        n->type = type_int;
    }
    return n;
}

AstNode *ast_member(AstNode *target, const char *member_name, int is_arrow, const char *file, int line) {
    AstNode *n = ast_new(AST_MEMBER, file, line);
    Type *struct_type = target->type;
    Member *mem;

    if (is_arrow) {
        if (!type_is_pointer(struct_type)) {
            c90_error(file, line, "arrow operator requires pointer to struct/union");
        }
        struct_type = struct_type->base;
    }

    mem = type_find_member(struct_type, member_name);
    if (!mem) {
        c90_error(file, line, "no member named '%s' in struct/union", member_name);
    }

    n->type = mem->type;
    n->u.member.target = target;
    n->u.member.member_name = c90_strdup(member_name);
    n->u.member.is_arrow = is_arrow;
    n->u.member.member = mem;
    return n;
}

/* ========================================================================= */
/* Parser Helper Functions                                                   */
/* ========================================================================= */

Parser *parser_new(Token *tokens) {
    Parser *p = (Parser *)c90_malloc(sizeof(Parser));
    p->tokens = tokens;
    p->current = tokens;
    p->globals = vec_new();
    p->strings = vec_new();
    p->floats = vec_new();
    p->current_func = NULL;
    p->current_func_locals = NULL;
    p->current_stack_offset = 0;
    p->label_seq = 1;
    p->switch_stack = vec_new();
    return p;
}

static Token *peek(Parser *p) {
    return p->current;
}

static Token *next(Parser *p) {
    Token *tok = p->current;
    if (p->current && p->current->kind != TOK_EOF) {
        p->current = p->current->next;
    }
    return tok;
}

static int match(Parser *p, int kind) {
    if (p->current && p->current->kind == kind) {
        next(p);
        return 1;
    }
    return 0;
}

static Token *expect(Parser *p, int kind) {
    Token *tok = p->current;
    if (!tok || tok->kind != kind) {
        c90_error(tok ? tok->filename : "<unknown>", tok ? tok->line : 0,
                  "expected '%s' but found '%s'",
                  token_kind_str(kind),
                  tok ? token_kind_str(tok->kind) : "end of file");
    }
    return next(p);
}

static char *gen_label(Parser *p, const char *prefix) {
    char buf[64];
    sprintf(buf, ".L_%s_%d", prefix, p->label_seq++);
    return c90_strdup(buf);
}

static void skip_attribute(Parser *p) {
    while (peek(p) && (peek(p)->kind == TOK_ATTRIBUTE || peek(p)->kind == TOK_EXTENSION || peek(p)->kind == TOK_ASM)) {
        if (match(p, TOK_ATTRIBUTE) || match(p, TOK_ASM)) {
            if (match(p, '(')) {
                int depth = 1;
                while (depth > 0 && peek(p) && peek(p)->kind != TOK_EOF) {
                    if (peek(p)->kind == '(') depth++;
                    else if (peek(p)->kind == ')') depth--;
                    next(p);
                }
            }
        } else if (match(p, TOK_EXTENSION)) {
            /* skip */
        }
    }
}

static int is_type_specifier(Parser *p) {
    Token *tok = peek(p);
    Symbol *sym;
    if (!tok) return 0;
    switch (tok->kind) {
        case TOK_VOID: case TOK_CHAR: case TOK_SHORT: case TOK_INT:
        case TOK_LONG: case TOK_FLOAT: case TOK_DOUBLE: case TOK_SIGNED:
        case TOK_UNSIGNED: case TOK_STRUCT: case TOK_UNION: case TOK_ENUM:
        case TOK_CONST: case TOK_VOLATILE:
        case TOK_AUTO: case TOK_REGISTER: case TOK_STATIC: case TOK_EXTERN:
        case TOK_TYPEDEF:
        case TOK_ATTRIBUTE: case TOK_EXTENSION: case TOK_INLINE:
        case TOK_RESTRICT: case TOK_ASM: case TOK_BUILTIN_VA_LIST:
        case TOK_COMPLEX: case TOK_BOOL:
            return 1;
        case TOK_IDENT:
            sym = scope_lookup(tok->str);
            return (sym && sym->kind == SYM_TYPEDEF);
        default:
            return 0;
    }
}

/* ========================================================================= */
/* Expression Parsing (Precedence Hierarchy)                                 */
/* ========================================================================= */

static AstNode *parse_primary(Parser *p) {
    Token *tok = peek(p);
    if (!tok) return NULL;

    if (tok->kind == TOK_INT_LIT) {
        Token *t_tok = next(p);
        Type *t;
        if (t_tok->is_unsigned) {
            t = ((unsigned long)t_tok->int_val > 4294967295UL) ? type_ulong : type_uint;
        } else {
            t = (t_tok->int_val > 2147483647L || t_tok->int_val < (-2147483647L - 1L)) ? type_long : type_int;
        }
        return ast_int_lit(t_tok->int_val, t_tok->is_unsigned, t, t_tok->filename, t_tok->line);
    }

    if (tok->kind == TOK_FLOAT_LIT) {
        Token *f_tok = next(p);
        AstNode *node;
        Type *t = type_double;
        if (f_tok->is_unsigned == 1) t = type_float;
        else if (f_tok->is_unsigned == 2) t = type_ldouble;
        node = ast_float_lit(t, f_tok->filename, f_tok->line);
        node->u.float_val.label = gen_label(p, "flt");
        if (t == type_float) {
            soft_strto_f32(f_tok->str, &node->u.float_val.u128_words[0]);
        } else if (t == type_ldouble) {
            soft_strto_f128(f_tok->str, (soft_f128 *)node->u.float_val.u128_words);
        } else {
            soft_strto_f64(f_tok->str, (void *)&node->u.float_val.u128_words[0]);
        }
        vec_push(p->floats, node);
        return node;
    }

    if (tok->kind == TOK_CHAR_LIT) {
        Token *c_tok = next(p);
        return ast_char_lit((int)c_tok->int_val, c_tok->filename, c_tok->line);
    }

    if (tok->kind == TOK_STR_LIT) {
        StrBuf *sb = strbuf_new();
        const char *file = tok->filename;
        int line = tok->line;
        AstNode *node;

        /* Adjacent string concatenation */
        while (peek(p) && peek(p)->kind == TOK_STR_LIT) {
            Token *st = next(p);
            strbuf_append_buf(sb, st->str, (int)st->int_val);
        }

        node = ast_str_lit(sb->data, sb->length, file, line);
        node->u.str_val.label = gen_label(p, "str");
        vec_push(p->strings, node);
        strbuf_free(sb);
        return node;
    }

    if (tok->kind == TOK_IDENT) {
        Symbol *sym;
        if (strcmp(tok->str, "__func__") == 0 ||
            strcmp(tok->str, "__FUNCTION__") == 0 ||
            strcmp(tok->str, "__PRETTY_FUNCTION__") == 0) {
            const char *fname = p->current_func ? p->current_func->name : "";
            AstNode *node = ast_str_lit(fname, (int)strlen(fname), tok->filename, tok->line);
            node->u.str_val.label = gen_label(p, "str");
            vec_push(p->strings, node);
            next(p);
            return node;
        }

        if (strcmp(tok->str, "__builtin_va_arg") == 0) {
            Token *va_tok = next(p);
            AstNode *node;
            AstNode *ap;
            Type *target_type;
            expect(p, '(');
            ap = parse_assignment(p);
            expect(p, ',');
            target_type = parse_type_name(p);
            expect(p, ')');
            node = ast_new(AST_VA_ARG, va_tok->filename, va_tok->line);
            node->type = target_type;
            node->u.va_arg.ap = ap;
            return node;
        }

        sym = scope_lookup(tok->str);
        next(p);
        if (!sym) {
            /* Implicit function declaration */
            if (peek(p) && peek(p)->kind == '(') {
                Type *ft = type_func_new(type_int, NULL, 1);
                sym = symbol_new(SYM_FUNC, tok->str, ft);
                sym->is_global = 1;
                scope_add_symbol(sym);
            } else {
                c90_error(tok->filename, tok->line, "undeclared identifier '%s'", tok->str);
            }
        }
        if (sym->kind == SYM_ENUM_CONST) {
            return ast_int_lit(sym->enum_value, 0, type_int, tok->filename, tok->line);
        }
        return ast_var(sym, tok->filename, tok->line);
    }

    if (tok->kind == TOK_EXTENSION) {
        next(p);
        return parse_primary(p);
    }

    if (tok->kind == '(') {
        AstNode *expr;
        next(p);
        expr = parse_expr(p);
        expect(p, ')');
        return expr;
    }

    c90_error(tok->filename, tok->line, "unexpected token '%s' in primary expression", token_kind_str(tok->kind));
    return NULL;
}

static AstNode *parse_postfix(Parser *p) {
    AstNode *node = parse_primary(p);

    while (1) {
        Token *tok = peek(p);
        if (!tok) break;

        if (tok->kind == '[') {
            /* Array indexing: a[i] -> *(a + i) */
            AstNode *index;
            AstNode *add;
            next(p);
            index = parse_expr(p);
            expect(p, ']');
            add = ast_binary(AST_ADD, node, index, tok->filename, tok->line);
            node = ast_unary(AST_DEREF, add, tok->filename, tok->line);
        } else if (tok->kind == '(') {
            /* Function call */
            Vector *args = vec_new();
            next(p);
            if (peek(p) && peek(p)->kind != ')') {
                while (1) {
                    vec_push(args, parse_assignment(p));
                    if (!match(p, ',')) break;
                }
            }
            expect(p, ')');
            if (node->type) {
                Type *ft = node->type;
                if (ft->kind == TYPE_PTR && ft->base && ft->base->kind == TYPE_FUNC) {
                    ft = ft->base;
                }
                if (ft->kind == TYPE_FUNC && ft->params) {
                    int i;
                    for (i = 0; i < args->size && i < ft->params->size; i++) {
                        Param *param = (Param *)vec_get(ft->params, i);
                        AstNode *arg = (AstNode *)vec_get(args, i);
                        if (param->type && arg->type && (type_is_floating(param->type) || type_is_floating(arg->type)) && !type_equal(param->type, arg->type)) {
                            args->data[i] = ast_cast(param->type, arg, tok->filename, tok->line);
                        }
                    }
                }
            }
            node = ast_call(node, args, tok->filename, tok->line);
        } else if (tok->kind == '.') {
            Token *id_tok;
            next(p);
            id_tok = expect(p, TOK_IDENT);
            node = ast_member(node, id_tok->str, 0, tok->filename, tok->line);
        } else if (tok->kind == TOK_ARROW) {
            Token *id_tok;
            next(p);
            id_tok = expect(p, TOK_IDENT);
            node = ast_member(node, id_tok->str, 1, tok->filename, tok->line);
        } else if (tok->kind == TOK_INC) {
            next(p);
            node = ast_unary(AST_POST_INC, node, tok->filename, tok->line);
        } else if (tok->kind == TOK_DEC) {
            next(p);
            node = ast_unary(AST_POST_DEC, node, tok->filename, tok->line);
        } else {
            break;
        }
    }

    return node;
}

static AstNode *parse_unary(Parser *p) {
    Token *tok = peek(p);
    if (!tok) return NULL;

    if (tok->kind == TOK_INC) {
        next(p);
        return ast_unary(AST_PRE_INC, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_DEC) {
        next(p);
        return ast_unary(AST_PRE_DEC, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '+') {
        next(p);
        return ast_unary(AST_POS, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '-') {
        next(p);
        return ast_unary(AST_NEG, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '~') {
        next(p);
        return ast_unary(AST_BITNOT, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '!') {
        next(p);
        return ast_unary(AST_LOGNOT, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '&') {
        next(p);
        return ast_unary(AST_ADDR, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == '*') {
        next(p);
        return ast_unary(AST_DEREF, parse_unary(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_SIZEOF) {
        AstNode *node;
        next(p);
        if (peek(p) && peek(p)->kind == '(') {
            Token *saved = p->current;
            next(p); /* skip '(' */
            if (is_type_specifier(p)) {
                Type *t = parse_type_name(p);
                expect(p, ')');
                node = ast_int_lit(t->size, 1, type_ulong, tok->filename, tok->line);
                return node;
            }
            p->current = saved;
        }
        {
            AstNode *op = parse_unary(p);
            node = ast_int_lit(op->type ? op->type->size : 4, 1, type_ulong, tok->filename, tok->line);
            return node;
        }
    }
    if (tok->kind == '(') {
        /* Check if cast: ( type-name ) unary-expr */
        Token *saved = p->current;
        next(p); /* skip '(' */
        if (is_type_specifier(p)) {
            Type *t = parse_type_name(p);
            if (match(p, ')')) {
                AstNode *op = parse_unary(p);
                return ast_cast(t, op, tok->filename, tok->line);
            }
        }
        p->current = saved;
    }

    return parse_postfix(p);
}

static AstNode *parse_multiplicative(Parser *p) {
    AstNode *node = parse_unary(p);
    while (1) {
        Token *tok = peek(p);
        if (!tok) break;
        if (tok->kind == '*') {
            next(p);
            node = ast_binary(AST_MUL, node, parse_unary(p), tok->filename, tok->line);
        } else if (tok->kind == '/') {
            next(p);
            node = ast_binary(AST_DIV, node, parse_unary(p), tok->filename, tok->line);
        } else if (tok->kind == '%') {
            next(p);
            node = ast_binary(AST_MOD, node, parse_unary(p), tok->filename, tok->line);
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parse_additive(Parser *p) {
    AstNode *node = parse_multiplicative(p);
    while (1) {
        Token *tok = peek(p);
        if (!tok) break;
        if (tok->kind == '+') {
            next(p);
            node = ast_binary(AST_ADD, node, parse_multiplicative(p), tok->filename, tok->line);
        } else if (tok->kind == '-') {
            next(p);
            node = ast_binary(AST_SUB, node, parse_multiplicative(p), tok->filename, tok->line);
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parse_shift(Parser *p) {
    AstNode *node = parse_additive(p);
    while (1) {
        Token *tok = peek(p);
        if (!tok) break;
        if (tok->kind == TOK_SHL) {
            next(p);
            node = ast_binary(AST_SHL, node, parse_additive(p), tok->filename, tok->line);
        } else if (tok->kind == TOK_SHR) {
            next(p);
            node = ast_binary(AST_SHR, node, parse_additive(p), tok->filename, tok->line);
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parse_relational(Parser *p) {
    AstNode *node = parse_shift(p);
    while (1) {
        Token *tok = peek(p);
        if (!tok) break;
        if (tok->kind == '<') {
            next(p);
            node = ast_binary(AST_LT, node, parse_shift(p), tok->filename, tok->line);
        } else if (tok->kind == '>') {
            next(p);
            node = ast_binary(AST_GT, node, parse_shift(p), tok->filename, tok->line);
        } else if (tok->kind == TOK_LE) {
            next(p);
            node = ast_binary(AST_LE, node, parse_shift(p), tok->filename, tok->line);
        } else if (tok->kind == TOK_GE) {
            next(p);
            node = ast_binary(AST_GE, node, parse_shift(p), tok->filename, tok->line);
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parse_equality(Parser *p) {
    AstNode *node = parse_relational(p);
    while (1) {
        Token *tok = peek(p);
        if (!tok) break;
        if (tok->kind == TOK_EQ) {
            next(p);
            node = ast_binary(AST_EQ, node, parse_relational(p), tok->filename, tok->line);
        } else if (tok->kind == TOK_NE) {
            next(p);
            node = ast_binary(AST_NE, node, parse_relational(p), tok->filename, tok->line);
        } else {
            break;
        }
    }
    return node;
}

static AstNode *parse_bitand(Parser *p) {
    AstNode *node = parse_equality(p);
    while (match(p, '&')) {
        Token *tok = p->current;
        node = ast_binary(AST_BITAND, node, parse_equality(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

static AstNode *parse_bitxor(Parser *p) {
    AstNode *node = parse_bitand(p);
    while (match(p, '^')) {
        Token *tok = p->current;
        node = ast_binary(AST_BITXOR, node, parse_bitand(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

static AstNode *parse_bitor(Parser *p) {
    AstNode *node = parse_bitxor(p);
    while (match(p, '|')) {
        Token *tok = p->current;
        node = ast_binary(AST_BITOR, node, parse_bitxor(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

static AstNode *parse_logical_and(Parser *p) {
    AstNode *node = parse_bitor(p);
    while (match(p, TOK_LAND)) {
        Token *tok = p->current;
        node = ast_binary(AST_LOGAND, node, parse_bitor(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

static AstNode *parse_logical_or(Parser *p) {
    AstNode *node = parse_logical_and(p);
    while (match(p, TOK_LOR)) {
        Token *tok = p->current;
        node = ast_binary(AST_LOGOR, node, parse_logical_and(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

static AstNode *parse_conditional(Parser *p) {
    AstNode *cond = parse_logical_or(p);
    if (match(p, '?')) {
        Token *tok = p->current;
        AstNode *then_expr = parse_expr(p);
        AstNode *else_expr;
        expect(p, ':');
        else_expr = parse_conditional(p);

        {
            AstNode *node = ast_new(AST_COND, tok ? tok->filename : NULL, tok ? tok->line : 0);
            node->type = type_max(then_expr->type, else_expr->type);
            node->u.cond.cond = cond;
            node->u.cond.then_expr = then_expr;
            node->u.cond.else_expr = else_expr;
            return node;
        }
    }
    return cond;
}

static AstNode *parse_assignment(Parser *p) {
    AstNode *lhs = parse_conditional(p);
    Token *tok = peek(p);
    if (!tok) return lhs;

    if (tok->kind == '=') {
        AstNode *rhs;
        next(p);
        rhs = parse_assignment(p);
        if (lhs->type && rhs->type && (type_is_floating(lhs->type) || type_is_floating(rhs->type)) && !type_equal(lhs->type, rhs->type)) {
            rhs = ast_cast(lhs->type, rhs, tok->filename, tok->line);
        }
        return ast_binary(AST_ASSIGN, lhs, rhs, tok->filename, tok->line);
    }
    if (tok->kind == TOK_ADD_ASSIGN) {
        next(p);
        return ast_binary(AST_ADD_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_SUB_ASSIGN) {
        next(p);
        return ast_binary(AST_SUB_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_MUL_ASSIGN) {
        next(p);
        return ast_binary(AST_MUL_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_DIV_ASSIGN) {
        next(p);
        return ast_binary(AST_DIV_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_MOD_ASSIGN) {
        next(p);
        return ast_binary(AST_MOD_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_SHL_ASSIGN) {
        next(p);
        return ast_binary(AST_SHL_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_SHR_ASSIGN) {
        next(p);
        return ast_binary(AST_SHR_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_AND_ASSIGN) {
        next(p);
        return ast_binary(AST_AND_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_XOR_ASSIGN) {
        next(p);
        return ast_binary(AST_XOR_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }
    if (tok->kind == TOK_OR_ASSIGN) {
        next(p);
        return ast_binary(AST_OR_ASSIGN, lhs, parse_assignment(p), tok->filename, tok->line);
    }

    return lhs;
}

static AstNode *parse_expr(Parser *p) {
    AstNode *node = parse_assignment(p);
    while (match(p, ',')) {
        Token *tok = p->current;
        node = ast_binary(AST_COMMA, node, parse_assignment(p), tok ? tok->filename : NULL, tok ? tok->line : 0);
    }
    return node;
}

/* ========================================================================= */
/* Declaration and Type Specifier Parsing                                    */
/* ========================================================================= */

static Type *parse_struct_union_body(Parser *p, int is_union, const char *tag) {
    Type *st;
    Member head;
    Member *tail = &head;
    int current_offset = 0;
    int current_bit_offset = 0;
    int last_unit_size = 0;
    int last_unit_offset = 0;
    int max_align = 1;

    if (tag) {
        st = scope_lookup_tag(tag);
        if (!st) {
            st = type_struct_new(tag, is_union);
            scope_add_tag(tag, st);
        }
    } else {
        st = type_struct_new(NULL, is_union);
    }

    head.next = NULL;
    expect(p, '{');

    while (peek(p) && peek(p)->kind != '}') {
        StorageClass sc;
        Type *base = parse_decl_specifiers(p, &sc);
        while (1) {
            char *mname = NULL;
            Type *mtype;
            Member *m;
            int is_bitfield = 0;
            int bit_width = 0;

            if (match(p, ';')) break;

            mtype = parse_declarator(p, base, &mname);

            /* Check for bitfield width: int field : width; or unnamed : width; */
            if (match(p, ':')) {
                AstNode *bw = parse_conditional(p);
                is_bitfield = 1;
                bit_width = (int)eval_const_expr(bw);
            }

            m = (Member *)c90_malloc(sizeof(Member));
            m->name = mname;
            m->type = mtype;
            m->bit_offset = 0;
            m->bit_width = bit_width;
            m->next = NULL;

            if (is_union) {
                m->offset = 0;
                m->bit_offset = 0;
                if (mtype->size > current_offset) current_offset = mtype->size;
                if (mtype->align > max_align) max_align = mtype->align;
            } else {
                if (is_bitfield) {
                    int unit_bits = (mtype && mtype->size > 0) ? mtype->size * 8 : 32;
                    if (bit_width == 0) {
                        /* Unnamed zero-width bitfield: align to next boundary */
                        if (current_bit_offset > 0) {
                            current_offset = last_unit_offset + last_unit_size;
                            current_bit_offset = 0;
                            last_unit_size = 0;
                        }
                    } else {
                        if (current_bit_offset > 0 &&
                            current_bit_offset + bit_width <= unit_bits &&
                            mtype->size == last_unit_size) {
                            m->offset = last_unit_offset;
                            m->bit_offset = current_bit_offset;
                            current_bit_offset += bit_width;
                        } else {
                            if (current_bit_offset > 0) {
                                current_offset = last_unit_offset + last_unit_size;
                                current_bit_offset = 0;
                            }
                            if (mtype->align > 1) {
                                current_offset = (current_offset + mtype->align - 1) & ~(mtype->align - 1);
                            }
                            last_unit_offset = current_offset;
                            last_unit_size = mtype->size;
                            m->offset = current_offset;
                            m->bit_offset = 0;
                            current_bit_offset = bit_width;
                            if (mtype->align > max_align) max_align = mtype->align;
                            if (last_unit_offset + last_unit_size > current_offset) {
                                current_offset = last_unit_offset + last_unit_size;
                            }
                        }
                    }
                } else {
                    if (current_bit_offset > 0) {
                        current_offset = last_unit_offset + last_unit_size;
                        current_bit_offset = 0;
                        last_unit_size = 0;
                    }
                    if (mtype->align > 1) {
                        current_offset = (current_offset + mtype->align - 1) & ~(mtype->align - 1);
                    }
                    m->offset = current_offset;
                    current_offset += mtype->size;
                    if (mtype->align > max_align) max_align = mtype->align;
                }
            }

            if (mname || (!is_bitfield)) {
                tail->next = m;
                tail = tail->next;
            }

            if (match(p, ';')) break;
            expect(p, ',');
        }
    }
    expect(p, '}');

    if (current_bit_offset > 0) {
        current_offset = last_unit_offset + last_unit_size;
    }

    /* Pad total size to alignment */
    if (max_align > 1) {
        current_offset = (current_offset + max_align - 1) & ~(max_align - 1);
    }
    st->size = current_offset;
    st->align = max_align;
    st->members = head.next;
    st->is_complete = 1;

    if (tag) {
        scope_add_tag(tag, st);
    }
    return st;
}

long eval_const_expr(AstNode *n) {
    if (!n) return 0;
    switch (n->kind) {
        case AST_INT_LIT:
        case AST_CHAR_LIT:
            return n->u.int_val.val;
        case AST_POS:
            return eval_const_expr(n->u.unop.operand);
        case AST_NEG:
            return -eval_const_expr(n->u.unop.operand);
        case AST_BITNOT:
            return ~eval_const_expr(n->u.unop.operand);
        case AST_LOGNOT:
            return !eval_const_expr(n->u.unop.operand);
        case AST_ADD:
            return eval_const_expr(n->u.binop.lhs) + eval_const_expr(n->u.binop.rhs);
        case AST_SUB:
            return eval_const_expr(n->u.binop.lhs) - eval_const_expr(n->u.binop.rhs);
        case AST_MUL:
            return eval_const_expr(n->u.binop.lhs) * eval_const_expr(n->u.binop.rhs);
        case AST_DIV: {
            long rhs = eval_const_expr(n->u.binop.rhs);
            return rhs != 0 ? eval_const_expr(n->u.binop.lhs) / rhs : 0;
        }
        case AST_MOD: {
            long rhs = eval_const_expr(n->u.binop.rhs);
            return rhs != 0 ? eval_const_expr(n->u.binop.lhs) % rhs : 0;
        }
        case AST_SHL:
            return eval_const_expr(n->u.binop.lhs) << eval_const_expr(n->u.binop.rhs);
        case AST_SHR:
            return eval_const_expr(n->u.binop.lhs) >> eval_const_expr(n->u.binop.rhs);
        case AST_BITAND:
            return eval_const_expr(n->u.binop.lhs) & eval_const_expr(n->u.binop.rhs);
        case AST_BITOR:
            return eval_const_expr(n->u.binop.lhs) | eval_const_expr(n->u.binop.rhs);
        case AST_BITXOR:
            return eval_const_expr(n->u.binop.lhs) ^ eval_const_expr(n->u.binop.rhs);
        case AST_EQ:
            return eval_const_expr(n->u.binop.lhs) == eval_const_expr(n->u.binop.rhs);
        case AST_NE:
            return eval_const_expr(n->u.binop.lhs) != eval_const_expr(n->u.binop.rhs);
        case AST_LT:
            return eval_const_expr(n->u.binop.lhs) < eval_const_expr(n->u.binop.rhs);
        case AST_LE:
            return eval_const_expr(n->u.binop.lhs) <= eval_const_expr(n->u.binop.rhs);
        case AST_GT:
            return eval_const_expr(n->u.binop.lhs) > eval_const_expr(n->u.binop.rhs);
        case AST_GE:
            return eval_const_expr(n->u.binop.lhs) >= eval_const_expr(n->u.binop.rhs);
        case AST_LOGAND:
            return eval_const_expr(n->u.binop.lhs) && eval_const_expr(n->u.binop.rhs);
        case AST_LOGOR:
            return eval_const_expr(n->u.binop.lhs) || eval_const_expr(n->u.binop.rhs);
        case AST_COND:
            return eval_const_expr(n->u.cond.cond) ? eval_const_expr(n->u.cond.then_expr) : eval_const_expr(n->u.cond.else_expr);
        case AST_CAST:
            return eval_const_expr(n->u.cast.operand);
        case AST_VAR:
            if (n->u.sym && n->u.sym->kind == SYM_ENUM_CONST) return n->u.sym->enum_value;
            return 0;
        default:
            return 0;
    }
}

int eval_const_float_expr(AstNode *n, unsigned int words[4], Type *type) {
    if (!n || !type) return 0;
    words[0] = words[1] = words[2] = words[3] = 0;

    if (n->kind == AST_FLOAT_LIT) {
        words[0] = n->u.float_val.u128_words[0];
        words[1] = n->u.float_val.u128_words[1];
        words[2] = n->u.float_val.u128_words[2];
        words[3] = n->u.float_val.u128_words[3];
        return 1;
    }

    if (n->kind == AST_INT_LIT || n->kind == AST_CHAR_LIT) {
        long val = n->u.int_val.val;
        if (type->kind == TYPE_FLOAT) {
            float f = (float)val;
            memcpy(&words[0], &f, 4);
        } else if (type->kind == TYPE_DOUBLE) {
            double d = (double)val;
            memcpy(&words[0], &d, 8);
        } else if (type->kind == TYPE_LDOUBLE) {
            long double ld = (long double)val;
            memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16);
        }
        return 1;
    }

    if (n->kind == AST_POS) {
        return eval_const_float_expr(n->u.unop.operand, words, type);
    }

    if (n->kind == AST_NEG) {
        int r = eval_const_float_expr(n->u.unop.operand, words, type);
        if (type->kind == TYPE_FLOAT) words[0] ^= 0x80000000U;
        else if (type->kind == TYPE_DOUBLE) words[1] ^= 0x80000000U;
        else if (type->kind == TYPE_LDOUBLE) words[3] ^= 0x80000000U;
        return r;
    }

    if (n->kind == AST_CAST) {
        unsigned int op_words[4];
        Type *src_type = n->u.cast.operand->type ? n->u.cast.operand->type : type_int;
        op_words[0] = op_words[1] = op_words[2] = op_words[3] = 0;
        if (type_is_floating(src_type)) {
            eval_const_float_expr(n->u.cast.operand, op_words, src_type);
            if (src_type->kind == TYPE_FLOAT) {
                float f; memcpy(&f, &op_words[0], 4);
                if (type->kind == TYPE_FLOAT) { memcpy(&words[0], &f, 4); }
                else if (type->kind == TYPE_DOUBLE) { double d = (double)f; memcpy(&words[0], &d, 8); }
                else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)f; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
            } else if (src_type->kind == TYPE_DOUBLE) {
                double d; memcpy(&d, &op_words[0], 8);
                if (type->kind == TYPE_FLOAT) { float f = (float)d; memcpy(&words[0], &f, 4); }
                else if (type->kind == TYPE_DOUBLE) { memcpy(&words[0], &d, 8); }
                else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)d; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
            } else if (src_type->kind == TYPE_LDOUBLE) {
                long double ld; memcpy(&ld, &op_words[0], sizeof(long double) <= 16 ? sizeof(long double) : 16);
                if (type->kind == TYPE_FLOAT) { float f = (float)ld; memcpy(&words[0], &f, 4); }
                else if (type->kind == TYPE_DOUBLE) { double d = (double)ld; memcpy(&words[0], &d, 8); }
                else if (type->kind == TYPE_LDOUBLE) { memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
            }
        } else {
            long l = eval_const_expr(n->u.cast.operand);
            if (type->kind == TYPE_FLOAT) { float f = (float)l; memcpy(&words[0], &f, 4); }
            else if (type->kind == TYPE_DOUBLE) { double d = (double)l; memcpy(&words[0], &d, 8); }
            else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)l; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
        }
        return 1;
    }

    if (n->kind == AST_ADD || n->kind == AST_SUB || n->kind == AST_MUL || n->kind == AST_DIV) {
        unsigned int w_lhs[4], w_rhs[4];
        Type *t = type_max(n->u.binop.lhs->type, n->u.binop.rhs->type);
        if (!t || !type_is_floating(t)) t = type;
        w_lhs[0] = w_lhs[1] = w_lhs[2] = w_lhs[3] = 0;
        w_rhs[0] = w_rhs[1] = w_rhs[2] = w_rhs[3] = 0;
        eval_const_float_expr(n->u.binop.lhs, w_lhs, t);
        eval_const_float_expr(n->u.binop.rhs, w_rhs, t);

        if (t->kind == TYPE_FLOAT) {
            float a, b, r = 0.0f;
            memcpy(&a, &w_lhs[0], 4);
            memcpy(&b, &w_rhs[0], 4);
            if (n->kind == AST_ADD) r = a + b;
            else if (n->kind == AST_SUB) r = a - b;
            else if (n->kind == AST_MUL) r = a * b;
            else if (n->kind == AST_DIV) r = (b != 0.0f) ? a / b : 0.0f;
            if (type->kind == TYPE_FLOAT) { memcpy(&words[0], &r, 4); }
            else if (type->kind == TYPE_DOUBLE) { double d = (double)r; memcpy(&words[0], &d, 8); }
            else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)r; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
        } else if (t->kind == TYPE_DOUBLE) {
            double a, b, r = 0.0;
            memcpy(&a, &w_lhs[0], 8);
            memcpy(&b, &w_rhs[0], 8);
            if (n->kind == AST_ADD) r = a + b;
            else if (n->kind == AST_SUB) r = a - b;
            else if (n->kind == AST_MUL) r = a * b;
            else if (n->kind == AST_DIV) r = (b != 0.0) ? a / b : 0.0;
            if (type->kind == TYPE_FLOAT) { float f = (float)r; memcpy(&words[0], &f, 4); }
            else if (type->kind == TYPE_DOUBLE) { memcpy(&words[0], &r, 8); }
            else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)r; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
        } else if (t->kind == TYPE_LDOUBLE) {
            long double a, b, r = 0.0L;
            memcpy(&a, &w_lhs[0], sizeof(long double) <= 16 ? sizeof(long double) : 16);
            memcpy(&b, &w_rhs[0], sizeof(long double) <= 16 ? sizeof(long double) : 16);
            if (n->kind == AST_ADD) r = a + b;
            else if (n->kind == AST_SUB) r = a - b;
            else if (n->kind == AST_MUL) r = a * b;
            else if (n->kind == AST_DIV) r = (b != 0.0L) ? a / b : 0.0L;
            if (type->kind == TYPE_FLOAT) { float f = (float)r; memcpy(&words[0], &f, 4); }
            else if (type->kind == TYPE_DOUBLE) { double d = (double)r; memcpy(&words[0], &d, 8); }
            else if (type->kind == TYPE_LDOUBLE) { memcpy(&words[0], &r, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
        }
        return 1;
    }

    if (n->kind == AST_VAR && n->u.sym && n->u.sym->kind == SYM_ENUM_CONST) {
        long val = n->u.sym->enum_value;
        if (type->kind == TYPE_FLOAT) { float f = (float)val; memcpy(&words[0], &f, 4); }
        else if (type->kind == TYPE_DOUBLE) { double d = (double)val; memcpy(&words[0], &d, 8); }
        else if (type->kind == TYPE_LDOUBLE) { long double ld = (long double)val; memcpy(&words[0], &ld, sizeof(long double) <= 16 ? sizeof(long double) : 16); }
        return 1;
    }

    return 0;
}

static Type *parse_enum_body(Parser *p, const char *tag) {
    Type *et = type_enum_new(tag);
    int current_val = 0;

    expect(p, '{');
    while (peek(p) && peek(p)->kind != '}') {
        Token *id_tok = expect(p, TOK_IDENT);
        Symbol *sym;

        if (match(p, '=')) {
            AstNode *expr = parse_conditional(p);
            current_val = (int)eval_const_expr(expr);
        }

        sym = symbol_new(SYM_ENUM_CONST, id_tok->str, et);
        sym->enum_value = current_val++;
        scope_add_symbol(sym);

        if (match(p, '}')) break;
        expect(p, ',');
    }

    if (tag) {
        scope_add_tag(tag, et);
    }
    return et;
}

static Type *parse_decl_specifiers(Parser *p, StorageClass *storage) {
    Type *type = NULL;
    int is_unsigned = 0;
    int is_long = 0;

    if (storage) *storage = STORAGE_AUTO;

    while (1) {
        Token *tok = peek(p);
        if (!tok) break;

        if (tok->kind == TOK_TYPEDEF) {
            if (storage) *storage = STORAGE_TYPEDEF;
            next(p);
        } else if (tok->kind == TOK_STATIC) {
            if (storage) *storage = STORAGE_STATIC;
            next(p);
        } else if (tok->kind == TOK_EXTERN) {
            if (storage) *storage = STORAGE_EXTERN;
            next(p);
        } else if (tok->kind == TOK_AUTO || tok->kind == TOK_REGISTER) {
            next(p);
        } else if (tok->kind == TOK_CONST || tok->kind == TOK_VOLATILE) {
            next(p);
        } else if (tok->kind == TOK_VOID) {
            type = type_void;
            next(p);
        } else if (tok->kind == TOK_CHAR) {
            type = is_unsigned ? type_uchar : type_char;
            next(p);
        } else if (tok->kind == TOK_SHORT) {
            type = is_unsigned ? type_ushort : type_short;
            next(p);
        } else if (tok->kind == TOK_INT) {
            if (type == type_short || type == type_ushort) {
                /* short int stays short */
            } else if (type == type_long || type == type_ulong || is_long) {
                /* long int stays long */
            } else {
                type = is_unsigned ? type_uint : type_int;
            }
            next(p);
        } else if (tok->kind == TOK_LONG) {
            if (type == type_double) {
                type = type_ldouble;
            } else {
                is_long = 1;
                type = is_unsigned ? type_ulong : type_long;
            }
            next(p);
        } else if (tok->kind == TOK_SIGNED) {
            is_unsigned = 0;
            next(p);
        } else if (tok->kind == TOK_UNSIGNED) {
            is_unsigned = 1;
            if (type == type_char) type = type_uchar;
            else if (type == type_short) type = type_ushort;
            else if (type == type_long) type = type_ulong;
            else type = type_uint;
            next(p);
        } else if (tok->kind == TOK_FLOAT) {
            type = type_float;
            next(p);
        } else if (tok->kind == TOK_DOUBLE) {
            if (is_long || type == type_long) {
                type = type_ldouble;
            } else {
                type = type_double;
            }
            next(p);
        } else if (tok->kind == TOK_ATTRIBUTE) {
            skip_attribute(p);
        } else if (tok->kind == TOK_EXTENSION || tok->kind == TOK_INLINE || tok->kind == TOK_RESTRICT || tok->kind == TOK_THREAD) {
            next(p);
        } else if (tok->kind == TOK_ASM) {
            next(p);
            if (match(p, '(')) {
                int depth = 1;
                while (depth > 0 && peek(p) && peek(p)->kind != TOK_EOF) {
                    if (peek(p)->kind == '(') depth++;
                    else if (peek(p)->kind == ')') depth--;
                    next(p);
                }
            }
        } else if (tok->kind == TOK_BUILTIN_VA_LIST) {
            type = type_pointer_to(type_void);
            next(p);
        } else if (tok->kind == TOK_COMPLEX) {
            next(p);
        } else if (tok->kind == TOK_BOOL) {
            type = type_char;
            next(p);
        } else if (tok->kind == TOK_STRUCT || tok->kind == TOK_UNION) {
            int is_union = (tok->kind == TOK_UNION);
            char *tag = NULL;
            next(p);
            if (peek(p) && peek(p)->kind == TOK_IDENT) {
                tag = next(p)->str;
            }
            if (peek(p) && peek(p)->kind == '{') {
                type = parse_struct_union_body(p, is_union, tag);
            } else if (tag) {
                type = scope_lookup_tag(tag);
                if (!type) {
                    type = type_struct_new(tag, is_union);
                    scope_add_tag(tag, type);
                }
            } else {
                c90_error(tok->filename, tok->line, "unnamed struct without definition");
            }
        } else if (tok->kind == TOK_ENUM) {
            char *tag = NULL;
            next(p);
            if (peek(p) && peek(p)->kind == TOK_IDENT) {
                tag = next(p)->str;
            }
            if (peek(p) && peek(p)->kind == '{') {
                type = parse_enum_body(p, tag);
            } else if (tag) {
                type = scope_lookup_tag(tag);
                if (!type) {
                    type = type_enum_new(tag);
                    scope_add_tag(tag, type);
                }
            }
        } else if (tok->kind == TOK_IDENT) {
            Symbol *sym = scope_lookup(tok->str);
            if (sym && sym->kind == SYM_TYPEDEF && !type) {
                type = sym->type;
                next(p);
            } else {
                break;
            }
        } else {
            break;
        }
    }

    if (!type) {
        type = is_unsigned ? type_uint : type_int;
    }
    return type;
}

typedef struct DeclaratorSuffix DeclaratorSuffix;
struct DeclaratorSuffix {
    enum { SUFFIX_ARRAY, SUFFIX_FUNC } kind;
    int array_len;
    Vector *params;
    int is_varargs;
};

typedef struct Declarator Declarator;
struct Declarator {
    int ptr_count;
    enum { DECL_IDENT, DECL_NESTED } kind;
    char *name;
    Declarator *nested;
    Vector *suffixes;
};

static Declarator *parse_declarator_node(Parser *p) {
    Declarator *d = (Declarator *)c90_malloc(sizeof(Declarator));
    d->kind = DECL_IDENT;
    d->ptr_count = 0;
    d->name = NULL;
    d->nested = NULL;
    d->suffixes = vec_new();

    skip_attribute(p);
    while (match(p, '*')) {
        d->ptr_count++;
        while (match(p, TOK_CONST) || match(p, TOK_VOLATILE) || match(p, TOK_RESTRICT));
        skip_attribute(p);
    }
    skip_attribute(p);

    if (peek(p) && peek(p)->kind == '(' && peek(p)->next &&
        (peek(p)->next->kind == '*' || peek(p)->next->kind == '(' ||
         (peek(p)->next->kind == TOK_IDENT && peek(p)->next->next &&
          (peek(p)->next->next->kind == ')' || peek(p)->next->next->kind == '[' || peek(p)->next->next->kind == '(')))) {
        next(p); /* skip '(' */
        d->kind = DECL_NESTED;
        d->nested = parse_declarator_node(p);
        expect(p, ')');
    } else if (peek(p) && peek(p)->kind == TOK_IDENT) {
        Token *tok = next(p);
        d->kind = DECL_IDENT;
        d->name = tok->str;
    } else {
        d->kind = DECL_IDENT;
        d->name = NULL;
    }
    skip_attribute(p);

    /* Parse direct declarator suffixes: [len] and (params) */
    while (1) {
        if (match(p, '[')) {
            DeclaratorSuffix *s = (DeclaratorSuffix *)c90_malloc(sizeof(DeclaratorSuffix));
            s->kind = SUFFIX_ARRAY;
            s->array_len = -1;
            s->params = NULL;
            s->is_varargs = 0;
            if (peek(p) && peek(p)->kind != ']') {
                AstNode *expr = parse_conditional(p);
                s->array_len = (int)eval_const_expr(expr);
            }
            expect(p, ']');
            vec_push(d->suffixes, s);
        } else if (match(p, '(')) {
            DeclaratorSuffix *s = (DeclaratorSuffix *)c90_malloc(sizeof(DeclaratorSuffix));
            s->kind = SUFFIX_FUNC;
            s->array_len = 0;
            s->params = vec_new();
            s->is_varargs = 0;

            if (peek(p) && peek(p)->kind != ')') {
                while (1) {
                    if (match(p, TOK_ELLIPSIS)) {
                        s->is_varargs = 1;
                        break;
                    }
                    {
                        StorageClass sc;
                        Type *ptype = parse_decl_specifiers(p, &sc);
                        char *pname = NULL;
                        Param *param = (Param *)c90_malloc(sizeof(Param));
                        ptype = parse_declarator(p, ptype, &pname);
                        param->name = pname;
                        param->type = type_decay(ptype);
                        vec_push(s->params, param);
                    }
                    if (!match(p, ',')) break;
                }
            }
            expect(p, ')');
            skip_attribute(p);
            vec_push(d->suffixes, s);
        } else {
            break;
        }
    }

    return d;
}

static Type *build_type_from_declarator(Declarator *d, Type *base, char **out_name) {
    Type *type = base;
    int i;
    while (d->ptr_count-- > 0) {
        type = type_pointer_to(type);
    }
    for (i = d->suffixes->size - 1; i >= 0; i--) {
        DeclaratorSuffix *s = (DeclaratorSuffix *)vec_get(d->suffixes, i);
        if (s->kind == SUFFIX_ARRAY) {
            type = type_array_of(type, s->array_len);
        } else if (s->kind == SUFFIX_FUNC) {
            type = type_func_new(type, s->params, s->is_varargs);
        }
    }
    if (d->kind == DECL_NESTED && d->nested) {
        return build_type_from_declarator(d->nested, type, out_name);
    }
    if (out_name) {
        *out_name = d->name;
    }
    return type;
}

static Type *parse_declarator(Parser *p, Type *base, char **out_name) {
    Declarator *d = parse_declarator_node(p);
    return build_type_from_declarator(d, base, out_name);
}

static Type *parse_type_name(Parser *p) {
    StorageClass sc;
    Type *base = parse_decl_specifiers(p, &sc);
    return parse_declarator(p, base, NULL);
}

static Initializer *parse_initializer(Parser *p, Type *type) {
    Initializer *init = (Initializer *)c90_malloc(sizeof(Initializer));
    init->expr = NULL;
    init->elements = NULL;
    init->is_compound = 0;
    init->type = type;
    init->offset = 0;

    if (match(p, '{')) {
        init->is_compound = 1;
        init->elements = vec_new();
        while (peek(p) && peek(p)->kind != '}') {
            Type *elem_type = (type && type->base) ? type->base : type_int;
            vec_push(init->elements, parse_initializer(p, elem_type));
            if (!match(p, ',')) break;
        }
        expect(p, '}');
        if (type && type->kind == TYPE_ARRAY && type->array_len < 0) {
            type->array_len = init->elements ? init->elements->size : 0;
            type->size = type->array_len * (type->base && type->base->size > 0 ? type->base->size : 1);
        }
    } else {
        init->expr = parse_assignment(p);
        if (init->expr && type && (type_is_floating(type) || type_is_floating(init->expr->type)) && !type_equal(type, init->expr->type)) {
            init->expr = ast_cast(type, init->expr, p->current ? p->current->filename : NULL, p->current ? p->current->line : 0);
        }
        if (type && type->kind == TYPE_ARRAY && type->array_len < 0 && init->expr && init->expr->kind == AST_STR_LIT) {
            type->array_len = init->expr->u.str_val.len + 1;
            type->size = type->array_len;
        }
    }
    return init;
}

/* ========================================================================= */
/* Statement Parsing                                                         */
/* ========================================================================= */

static AstNode *parse_stmt(Parser *p) {
    Token *tok = peek(p);
    if (!tok) return NULL;

    if (tok->kind == '{') {
        return parse_compound_stmt(p);
    }

    if (tok->kind == TOK_IF) {
        AstNode *node = ast_new(AST_IF, tok->filename, tok->line);
        next(p);
        expect(p, '(');
        node->u.if_stmt.cond = parse_expr(p);
        expect(p, ')');
        node->u.if_stmt.then_stmt = parse_stmt(p);
        if (match(p, TOK_ELSE)) {
            node->u.if_stmt.else_stmt = parse_stmt(p);
        } else {
            node->u.if_stmt.else_stmt = NULL;
        }
        return node;
    }

    if (tok->kind == TOK_WHILE) {
        AstNode *node = ast_new(AST_WHILE, tok->filename, tok->line);
        next(p);
        expect(p, '(');
        node->u.loop_stmt.cond = parse_expr(p);
        expect(p, ')');
        node->u.loop_stmt.break_label = gen_label(p, "brk");
        node->u.loop_stmt.continue_label = gen_label(p, "cnt");
        node->u.loop_stmt.body = parse_stmt(p);
        return node;
    }

    if (tok->kind == TOK_DO) {
        AstNode *node = ast_new(AST_DO_WHILE, tok->filename, tok->line);
        next(p);
        node->u.loop_stmt.break_label = gen_label(p, "brk");
        node->u.loop_stmt.continue_label = gen_label(p, "cnt");
        node->u.loop_stmt.body = parse_stmt(p);
        expect(p, TOK_WHILE);
        expect(p, '(');
        node->u.loop_stmt.cond = parse_expr(p);
        expect(p, ')');
        expect(p, ';');
        return node;
    }

    if (tok->kind == TOK_FOR) {
        AstNode *node = ast_new(AST_FOR, tok->filename, tok->line);
        next(p);
        expect(p, '(');
        if (peek(p) && peek(p)->kind != ';') {
            node->u.for_stmt.init = parse_expr(p);
        } else {
            node->u.for_stmt.init = NULL;
        }
        expect(p, ';');
        if (peek(p) && peek(p)->kind != ';') {
            node->u.for_stmt.cond = parse_expr(p);
        } else {
            node->u.for_stmt.cond = NULL;
        }
        expect(p, ';');
        if (peek(p) && peek(p)->kind != ')') {
            node->u.for_stmt.step = parse_expr(p);
        } else {
            node->u.for_stmt.step = NULL;
        }
        expect(p, ')');
        node->u.for_stmt.break_label = gen_label(p, "brk");
        node->u.for_stmt.continue_label = gen_label(p, "cnt");
        node->u.for_stmt.body = parse_stmt(p);
        return node;
    }

    if (tok->kind == TOK_SWITCH) {
        AstNode *node = ast_new(AST_SWITCH, tok->filename, tok->line);
        next(p);
        expect(p, '(');
        node->u.switch_stmt.cond = parse_expr(p);
        expect(p, ')');
        node->u.switch_stmt.cases = vec_new();
        node->u.switch_stmt.break_label = gen_label(p, "brk");
        node->u.switch_stmt.default_label = NULL;

        vec_push(p->switch_stack, node);
        node->u.switch_stmt.body = parse_stmt(p);
        vec_pop(p->switch_stack);
        return node;
    }

    if (tok->kind == TOK_CASE) {
        AstNode *node = ast_new(AST_CASE, tok->filename, tok->line);
        AstNode *val_node;
        next(p);
        val_node = parse_conditional(p);
        node->u.case_stmt.val = eval_const_expr(val_node);
        expect(p, ':');
        node->u.case_stmt.label = gen_label(p, "case");
        if (p->switch_stack->size > 0) {
            AstNode *sw = (AstNode *)vec_get(p->switch_stack, p->switch_stack->size - 1);
            vec_push(sw->u.switch_stmt.cases, node);
        }
        node->u.case_stmt.stmt = parse_stmt(p);
        return node;
    }

    if (tok->kind == TOK_DEFAULT) {
        AstNode *node = ast_new(AST_DEFAULT, tok->filename, tok->line);
        next(p);
        expect(p, ':');
        node->u.default_stmt.label = gen_label(p, "default");
        if (p->switch_stack->size > 0) {
            AstNode *sw = (AstNode *)vec_get(p->switch_stack, p->switch_stack->size - 1);
            sw->u.switch_stmt.default_label = node->u.default_stmt.label;
        }
        node->u.default_stmt.stmt = parse_stmt(p);
        return node;
    }

    if (tok->kind == TOK_BREAK) {
        AstNode *node = ast_new(AST_BREAK, tok->filename, tok->line);
        next(p);
        expect(p, ';');
        return node;
    }

    if (tok->kind == TOK_CONTINUE) {
        AstNode *node = ast_new(AST_CONTINUE, tok->filename, tok->line);
        next(p);
        expect(p, ';');
        return node;
    }

    if (tok->kind == TOK_RETURN) {
        AstNode *node = ast_new(AST_RETURN, tok->filename, tok->line);
        next(p);
        if (peek(p) && peek(p)->kind != ';') {
            node->u.return_stmt.expr = parse_expr(p);
            if (p->current_func && p->current_func->type && p->current_func->type->base &&
                node->u.return_stmt.expr && node->u.return_stmt.expr->type &&
                (type_is_floating(p->current_func->type->base) || type_is_floating(node->u.return_stmt.expr->type)) &&
                !type_equal(p->current_func->type->base, node->u.return_stmt.expr->type)) {
                node->u.return_stmt.expr = ast_cast(p->current_func->type->base, node->u.return_stmt.expr, tok->filename, tok->line);
            }
        } else {
            node->u.return_stmt.expr = NULL;
        }
        expect(p, ';');
        return node;
    }

    if (tok->kind == TOK_GOTO) {
        AstNode *node = ast_new(AST_GOTO, tok->filename, tok->line);
        Token *id;
        next(p);
        id = expect(p, TOK_IDENT);
        node->u.label_stmt.name = c90_strdup(id->str);
        expect(p, ';');
        return node;
    }

    if (tok->kind == TOK_IDENT && peek(p)->next && peek(p)->next->kind == ':') {
        Token *id = next(p);
        AstNode *node = ast_new(AST_LABEL, id->filename, id->line);
        node->u.label_stmt.name = c90_strdup(id->str);
        expect(p, ':');
        node->u.label_stmt.stmt = parse_stmt(p);
        return node;
    }

    if (match(p, ';')) {
        /* Null statement */
        AstNode *node = ast_new(AST_BLOCK, tok->filename, tok->line);
        node->u.block.stmts = vec_new();
        return node;
    }

    /* Expression statement */
    {
        AstNode *node = ast_new(AST_EXPR_STMT, tok->filename, tok->line);
        node->u.expr_stmt.expr = parse_expr(p);
        expect(p, ';');
        return node;
    }
}

static AstNode *parse_compound_stmt(Parser *p) {
    Token *tok = expect(p, '{');
    AstNode *block = ast_new(AST_BLOCK, tok->filename, tok->line);
    block->u.block.stmts = vec_new();

    scope_enter();

    while (peek(p) && peek(p)->kind != '}') {
        if (is_type_specifier(p)) {
            StorageClass storage;
            Type *base = parse_decl_specifiers(p, &storage);
            while (1) {
                char *name = NULL;
                Type *type = parse_declarator(p, base, &name);
                Symbol *sym;
                AstNode *decl_node;

                if (storage == STORAGE_TYPEDEF) {
                    sym = symbol_new(SYM_TYPEDEF, name, type);
                    scope_add_symbol(sym);
                    if (match(p, ';')) break;
                    expect(p, ',');
                    continue;
                }

                if (type->kind == TYPE_FUNC || storage == STORAGE_EXTERN) {
                    sym = symbol_new(type->kind == TYPE_FUNC ? SYM_FUNC : SYM_VAR, name, type);
                    sym->storage = storage;
                    sym->is_global = 1;
                    scope_add_symbol(sym);
                    if (match(p, ';')) break;
                    expect(p, ',');
                    continue;
                }

                sym = symbol_new(SYM_VAR, name, type);
                sym->storage = storage;
                scope_add_symbol(sym);
                if (storage == STORAGE_STATIC) {
                    sym->asm_label = gen_label(p, name);
                    if (match(p, '=')) {
                        sym->init = parse_initializer(p, type);
                        sym->is_defined = 1;
                    }
                    vec_push(p->globals, sym);
                } else {
                    Initializer *local_init = NULL;
                    if (match(p, '=')) {
                        local_init = parse_initializer(p, type);
                    }
#if defined(TARGET_I386) || defined(TARGET_RISCV32) || defined(TARGET_32BIT)
                    p->current_stack_offset += (type->size + 3) & ~3; /* 4-byte aligned stack slots */
#else
                    p->current_stack_offset += (type->size + 7) & ~7; /* 8-byte aligned stack slots */
#endif
                    sym->stack_offset = -p->current_stack_offset;
                    if (p->current_func_locals) {
                        vec_push(p->current_func_locals, sym);
                    }
                    decl_node = ast_new(AST_DECL_STMT, tok->filename, tok->line);
                    decl_node->u.decl_stmt.sym = sym;
                    decl_node->u.decl_stmt.init = local_init;
                    vec_push(block->u.block.stmts, decl_node);
                }

                if (match(p, ';')) break;
                expect(p, ',');
            }
        } else {
            vec_push(block->u.block.stmts, parse_stmt(p));
        }
    }

    expect(p, '}');
    scope_exit();
    return block;
}

/* ========================================================================= */
/* Top-Level Parsing (Translation Unit)                                      */
/* ========================================================================= */

AstNode *parser_parse(Parser *p) {
    AstNode *unit = ast_new(AST_TRANSLATION_UNIT, p->tokens ? p->tokens->filename : "<input>", 1);
    unit->u.trans_unit.decls = vec_new();
    unit->u.trans_unit.globals = p->globals;
    unit->u.trans_unit.strings = p->strings;
    unit->u.trans_unit.floats = p->floats;

    scope_enter(); /* Global scope */

    while (peek(p) && peek(p)->kind != TOK_EOF) {
        StorageClass storage;
        Type *base;
        char *name = NULL;
        Type *type;

        if (match(p, ';')) continue;

        base = parse_decl_specifiers(p, &storage);
        if (peek(p) && peek(p)->kind == ';') {
            /* struct/union/enum definition alone */
            next(p);
            continue;
        }

        type = parse_declarator(p, base, &name);
        skip_attribute(p);

        if (storage == STORAGE_TYPEDEF) {
            Symbol *sym = symbol_new(SYM_TYPEDEF, name, type);
            scope_add_symbol(sym);
            while (match(p, ',')) {
                char *tname = NULL;
                Type *ttype = parse_declarator(p, base, &tname);
                skip_attribute(p);
                sym = symbol_new(SYM_TYPEDEF, tname, ttype);
                scope_add_symbol(sym);
            }
            skip_attribute(p);
            expect(p, ';');
            continue;
        }

        /* Check for Function Definition */
        if (type->kind == TYPE_FUNC && peek(p) && peek(p)->kind == '{') {
            Symbol *sym = scope_lookup(name);
            AstNode *func_node = ast_new(AST_FUNC_DEF, peek(p)->filename, peek(p)->line);
            int i;

            if (!sym) {
                sym = symbol_new(SYM_FUNC, name, type);
                sym->storage = storage;
                sym->is_global = (storage != STORAGE_STATIC);
                sym->is_defined = 1;
                scope_add_symbol(sym);
            } else {
                sym->is_defined = 1;
            }

            p->current_func = sym;
            p->current_func_locals = vec_new();
#ifdef TARGET_RISCV32
            p->current_stack_offset = 32;
#elif defined(TARGET_I386)
            p->current_stack_offset = 0;
#else
            p->current_stack_offset = (type && type->is_varargs) ? 192 : 48;
#endif

            func_node->u.func_def.sym = sym;
            func_node->u.func_def.params = vec_new();
            func_node->u.func_def.locals = p->current_func_locals;

            scope_enter(); /* Function parameter & body scope */

            /* Add function parameters as local symbols */
            if (type->params) {
#ifdef TARGET_RISCV32
                for (i = 0; i < type->params->size; i++) {
                    Param *param = (Param *)vec_get(type->params, i);
                    if (param->name) {
                        Symbol *psym = symbol_new(SYM_VAR, param->name, param->type);
                        int psize = (param->type->size + 3) & ~3;
                        if (psize < 4) psize = 4;
                        p->current_stack_offset += psize;
                        psym->stack_offset = -p->current_stack_offset;
                        scope_add_symbol(psym);
                        vec_push(func_node->u.func_def.params, psym);
                        vec_push(p->current_func_locals, psym);
                    }
                }
#elif defined(TARGET_I386)
                int param_offset = 8;
                for (i = 0; i < type->params->size; i++) {
                    Param *param = (Param *)vec_get(type->params, i);
                    if (param->name) {
                        Symbol *psym = symbol_new(SYM_VAR, param->name, param->type);
                        int psize = 4;
                        if (param->type && (param->type->kind == TYPE_STRUCT || param->type->kind == TYPE_UNION || param->type->kind == TYPE_DOUBLE || param->type->kind == TYPE_LDOUBLE)) {
                            psize = (param->type->size + 3) & ~3;
                            if (psize < 4) psize = 4;
                        }
                        psym->stack_offset = param_offset;
                        param_offset += psize;
                        scope_add_symbol(psym);
                        vec_push(func_node->u.func_def.params, psym);
                        vec_push(p->current_func_locals, psym);
                    }
                }
#else
                int reg_idx = 0;
                int stack_idx = 0;
                for (i = 0; i < type->params->size; i++) {
                    Param *param = (Param *)vec_get(type->params, i);
                    if (param->name) {
                        Symbol *psym = symbol_new(SYM_VAR, param->name, param->type);
                        int psize = 8;
                        int words = 1;
                        if (param->type && (param->type->kind == TYPE_STRUCT || param->type->kind == TYPE_UNION)) {
                            psize = (param->type->size + 7) & ~7;
                            if (psize < 8) psize = 8;
                            words = psize / 8;
                        }
                        if (reg_idx + words <= 6) {
                            p->current_stack_offset += psize;
                            psym->stack_offset = -p->current_stack_offset;
                            reg_idx += words;
                        } else {
                            /* System V AMD64 ABI stack arguments */
                            psym->stack_offset = 16 + stack_idx * 8;
                            stack_idx += words;
                        }
                        scope_add_symbol(psym);
                        vec_push(func_node->u.func_def.params, psym);
                        vec_push(p->current_func_locals, psym);
                    }
                }
#endif
            }

#if !defined(TARGET_I386) && !defined(TARGET_RISCV32)
            if (type->is_varargs) {
                int start_reg = type->params ? type->params->size : 0;
                while (start_reg < 6) {
                    p->current_stack_offset += 8;
                    start_reg++;
                }
            }
#endif

            func_node->u.func_def.body = parse_compound_stmt(p);
            func_node->u.func_def.stack_size = (p->current_stack_offset + 160 + 15) & ~15; /* 16-byte align */

            scope_exit();
            p->current_func = NULL;
            p->current_func_locals = NULL;

            vec_push(unit->u.trans_unit.decls, func_node);
            continue;
        }

        /* Global Variable / Function Declaration */
        while (1) {
            Symbol *sym = scope_lookup_current(name);
            if (!sym) {
                sym = symbol_new(type->kind == TYPE_FUNC ? SYM_FUNC : SYM_VAR, name, type);
                sym->storage = storage;
                sym->is_global = 1;
                scope_add_symbol(sym);
                if (type->kind != TYPE_FUNC) {
                    vec_push(p->globals, sym);
                }
            } else {
                if (storage != STORAGE_EXTERN) {
                    sym->storage = storage;
                    sym->is_global = 1;
                }
                if (type->kind != TYPE_FUNC && sym->storage != STORAGE_EXTERN) {
                    int already_in_globals = 0;
                    int k;
                    for (k = 0; k < p->globals->size; k++) {
                        if (vec_get(p->globals, k) == sym) {
                            already_in_globals = 1;
                            break;
                        }
                    }
                    if (!already_in_globals) {
                        vec_push(p->globals, sym);
                    }
                }
            }

            if (match(p, '=')) {
                sym->init = parse_initializer(p, type);
                sym->is_defined = 1;
            }

            skip_attribute(p);
            if (match(p, ';')) break;
            expect(p, ',');
            name = NULL;
            type = parse_declarator(p, base, &name);
            skip_attribute(p);
        }
    }

    scope_exit();
    return unit;
}
