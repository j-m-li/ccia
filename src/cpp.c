/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

/* ========================================================================= */
/* Preprocessor State and Helper Functions                                   */
/* ========================================================================= */

typedef struct CondState {
    int active;       /* Is current branch active and emitting tokens? */
    int parent_active;/* Was the parent enclosing condition active? */
    int branch_taken; /* Has any if/elif branch been taken already? */
} CondState;

static char *read_entire_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;
    char *buf;
    size_t read_bytes;

    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = (char *)c90_malloc(size + 1);
    read_bytes = fread(buf, 1, size, fp);
    buf[read_bytes] = '\0';
    fclose(fp);
    return buf;
}

Preprocessor *cpp_new(void) {
    Preprocessor *cpp = (Preprocessor *)c90_malloc(sizeof(Preprocessor));
    cpp->macros = map_new();
    cpp->include_paths = vec_new();
    cpp->cond_stack = vec_new();
    cpp->current_file = NULL;
    cpp->current_line = 0;

    /* Add current directory as initial include search path */
    vec_push(cpp->include_paths, c90_strdup("."));

    /* Predefine standard C90 macros */
#ifdef TARGET_I386
    cpp_define_macro(cpp, "__STDC__", "1");
    cpp_define_macro(cpp, "__STRICT_ANSI__", "1");
    cpp_define_macro(cpp, "__WORDSIZE", "32");
    cpp_define_macro(cpp, "__i386", "1");
    cpp_define_macro(cpp, "__i386__", "1");
    cpp_define_macro(cpp, "__x86__", "1");
    cpp_define_macro(cpp, "__i686", "1");
    cpp_define_macro(cpp, "__i686__", "1");
    cpp_define_macro(cpp, "__ILP32__", "1");
    cpp_define_macro(cpp, "_ILP32", "1");
    cpp_define_macro(cpp, "__linux__", "1");
    cpp_define_macro(cpp, "__unix__", "1");
    cpp_define_macro(cpp, "__GNUC__", "4");
    cpp_define_macro(cpp, "__GNUC_MINOR__", "9");
    cpp_define_macro(cpp, "__GNUC_PATCHLEVEL__", "0");
    cpp_define_macro(cpp, "__CC90__", "1");
    cpp_define_macro(cpp, "__CC90_I386__", "1");
    cpp_define_macro(cpp, "__CC90_VERSION__", "\"1.0.0\"");
    cpp_define_macro(cpp, "__SIZE_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__PTRDIFF_TYPE__", "int");
    cpp_define_macro(cpp, "__INTPTR_TYPE__", "int");
    cpp_define_macro(cpp, "__UINTPTR_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__INT8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT16_TYPE__", "short");
    cpp_define_macro(cpp, "__INT32_TYPE__", "int");
    cpp_define_macro(cpp, "__INT64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT16_TYPE__", "unsigned short");
    cpp_define_macro(cpp, "__UINT32_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INT_LEAST8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT_LEAST16_TYPE__", "short");
    cpp_define_macro(cpp, "__INT_LEAST32_TYPE__", "int");
    cpp_define_macro(cpp, "__INT_LEAST64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT_LEAST8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT_LEAST16_TYPE__", "unsigned short");
    cpp_define_macro(cpp, "__UINT_LEAST32_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT_LEAST64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INT_FAST8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT_FAST16_TYPE__", "int");
    cpp_define_macro(cpp, "__INT_FAST32_TYPE__", "int");
    cpp_define_macro(cpp, "__INT_FAST64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT_FAST8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT_FAST16_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT_FAST32_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT_FAST64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INTMAX_TYPE__", "long");
    cpp_define_macro(cpp, "__UINTMAX_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__WCHAR_TYPE__", "long");
    cpp_define_macro(cpp, "__WINT_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__CHAR_BIT__", "8");
    cpp_define_macro(cpp, "__SCHAR_MAX__", "127");
    cpp_define_macro(cpp, "__SHRT_MAX__", "32767");
    cpp_define_macro(cpp, "__INT_MAX__", "2147483647");
    cpp_define_macro(cpp, "__LONG_MAX__", "2147483647L");
    cpp_define_macro(cpp, "__LONG_LONG_MAX__", "2147483647L");
    cpp_define_macro(cpp, "__SIZE_MAX__", "4294967295U");
    cpp_define_macro(cpp, "__PTRDIFF_MAX__", "2147483647");
    cpp_define_macro(cpp, "__INTPTR_MAX__", "2147483647");
    cpp_define_macro(cpp, "__UINTPTR_MAX__", "4294967295U");
    cpp_define_macro(cpp, "STBI_NO_SIMD", "1");
#else
    cpp_define_macro(cpp, "__STDC__", "1");
    cpp_define_macro(cpp, "__STRICT_ANSI__", "1");
    cpp_define_macro(cpp, "__WORDSIZE", "64");
    cpp_define_macro(cpp, "__WORDSIZE_TIME64_COMPAT32", "1");
    cpp_define_macro(cpp, "__SYSCALL_WORDSIZE", "64");
    cpp_define_macro(cpp, "__x86_64", "1");
    cpp_define_macro(cpp, "__x86_64__", "1");
    cpp_define_macro(cpp, "__amd64", "1");
    cpp_define_macro(cpp, "__amd64__", "1");
    cpp_define_macro(cpp, "__LP64__", "1");
    cpp_define_macro(cpp, "_LP64", "1");
    cpp_define_macro(cpp, "__linux__", "1");
    cpp_define_macro(cpp, "__unix__", "1");
    cpp_define_macro(cpp, "__GNUC__", "4");
    cpp_define_macro(cpp, "__GNUC_MINOR__", "9");
    cpp_define_macro(cpp, "__GNUC_PATCHLEVEL__", "0");
    cpp_define_macro(cpp, "__CC90__", "1");
    cpp_define_macro(cpp, "__CC90_VERSION__", "\"1.0.0\"");
    cpp_define_macro(cpp, "__SIZE_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__PTRDIFF_TYPE__", "long");
    cpp_define_macro(cpp, "__INTPTR_TYPE__", "long");
    cpp_define_macro(cpp, "__UINTPTR_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INT8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT16_TYPE__", "short");
    cpp_define_macro(cpp, "__INT32_TYPE__", "int");
    cpp_define_macro(cpp, "__INT64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT16_TYPE__", "unsigned short");
    cpp_define_macro(cpp, "__UINT32_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INT_LEAST8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT_LEAST16_TYPE__", "short");
    cpp_define_macro(cpp, "__INT_LEAST32_TYPE__", "int");
    cpp_define_macro(cpp, "__INT_LEAST64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT_LEAST8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT_LEAST16_TYPE__", "unsigned short");
    cpp_define_macro(cpp, "__UINT_LEAST32_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__UINT_LEAST64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INT_FAST8_TYPE__", "signed char");
    cpp_define_macro(cpp, "__INT_FAST16_TYPE__", "long");
    cpp_define_macro(cpp, "__INT_FAST32_TYPE__", "long");
    cpp_define_macro(cpp, "__INT_FAST64_TYPE__", "long");
    cpp_define_macro(cpp, "__UINT_FAST8_TYPE__", "unsigned char");
    cpp_define_macro(cpp, "__UINT_FAST16_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__UINT_FAST32_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__UINT_FAST64_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__INTMAX_TYPE__", "long");
    cpp_define_macro(cpp, "__UINTMAX_TYPE__", "unsigned long");
    cpp_define_macro(cpp, "__WCHAR_TYPE__", "int");
    cpp_define_macro(cpp, "__WINT_TYPE__", "unsigned int");
    cpp_define_macro(cpp, "__CHAR_BIT__", "8");
    cpp_define_macro(cpp, "__SCHAR_MAX__", "127");
    cpp_define_macro(cpp, "__SHRT_MAX__", "32767");
    cpp_define_macro(cpp, "__INT_MAX__", "2147483647");
    cpp_define_macro(cpp, "__LONG_MAX__", "9223372036854775807L");
    cpp_define_macro(cpp, "__LONG_LONG_MAX__", "9223372036854775807L");
    cpp_define_macro(cpp, "__SIZE_MAX__", "18446744073709551615UL");
    cpp_define_macro(cpp, "__PTRDIFF_MAX__", "9223372036854775807L");
    cpp_define_macro(cpp, "__INTPTR_MAX__", "9223372036854775807L");
    cpp_define_macro(cpp, "__UINTPTR_MAX__", "18446744073709551615UL");
    cpp_define_macro(cpp, "STBI_NO_SIMD", "1");
#endif

    return cpp;
}

void cpp_add_include_path(Preprocessor *cpp, const char *path) {
    vec_push(cpp->include_paths, c90_strdup(path));
}

void cpp_define_macro(Preprocessor *cpp, const char *name, const char *body) {
    Macro *m = (Macro *)c90_malloc(sizeof(Macro));
    m->name = c90_strdup(name);
    m->is_function_like = 0;
    m->params = NULL;
    m->body = vec_new();
    m->is_builtin = 0;

    if (body) {
        Lexer *l = lexer_new(body, "<macro-def>");
        Token *toks = lexer_tokenize(l);
        while (toks && toks->kind != TOK_EOF) {
            vec_push(m->body, toks);
            toks = toks->next;
        }
        free(l);
    }
    map_put(cpp->macros, name, m);
}

void cpp_undef_macro(Preprocessor *cpp, const char *name) {
    map_put(cpp->macros, name, NULL);
}

static Token *clone_token(Token *t) {
    Token *tok;
    if (!t) return NULL;
    tok = (Token *)c90_malloc(sizeof(Token));
    tok->kind = t->kind;
    tok->str = t->str ? c90_strdup(t->str) : NULL;
    tok->int_val = t->int_val;
    tok->is_unsigned = t->is_unsigned;
    tok->filename = t->filename ? c90_strdup(t->filename) : NULL;
    tok->line = t->line;
    tok->col = t->col;
    tok->hideset = NULL;
    if (t->hideset) {
        int i;
        tok->hideset = vec_new();
        for (i = 0; i < t->hideset->size; i++) {
            vec_push(tok->hideset, c90_strdup((char *)vec_get(t->hideset, i)));
        }
    }
    tok->next = NULL;
    return tok;
}

static int hideset_contains(Vector *hs, const char *name) {
    int i;
    if (!hs || !name) return 0;
    for (i = 0; i < hs->size; i++) {
        char *n = (char *)vec_get(hs, i);
        if (n && strcmp(n, name) == 0) return 1;
    }
    return 0;
}

static void hideset_add(Vector **phs, const char *name) {
    if (!name) return;
    if (!*phs) *phs = vec_new();
    if (!hideset_contains(*phs, name)) {
        vec_push(*phs, c90_strdup(name));
    }
}

/* ========================================================================= */
/* Macro Expansion & Parameter Substitution                                  */
/* ========================================================================= */

static Vector *expand_macro(Preprocessor *cpp, Macro *m, Vector *args);

static Vector *expand_token_vector(Preprocessor *cpp, Vector *tokens) {
    Vector *out = vec_new();
    Token *cur;
    int k;
    if (!tokens || tokens->size == 0) return out;

    for (k = 0; k < tokens->size - 1; k++) {
        Token *t1 = (Token *)vec_get(tokens, k);
        Token *t2 = (Token *)vec_get(tokens, k + 1);
        t1->next = t2;
    }
    ((Token *)vec_get(tokens, tokens->size - 1))->next = NULL;
    cur = (Token *)vec_get(tokens, 0);

    while (cur && cur->kind != TOK_EOF) {
        Macro *m = NULL;
        if (cur->kind == TOK_IDENT && !hideset_contains(cur->hideset, cur->str)) {
            m = (Macro *)map_get(cpp->macros, cur->str);
        }
        if (m) {
            if (!m->is_function_like) {
                Vector *exp = expand_macro(cpp, m, NULL);
                Token *next_tok = cur->next;
                if (exp->size > 0) {
                    for (k = 0; k < exp->size - 1; k++) {
                        ((Token *)vec_get(exp, k))->next = (Token *)vec_get(exp, k + 1);
                    }
                    ((Token *)vec_get(exp, exp->size - 1))->next = next_tok;
                    cur = (Token *)vec_get(exp, 0);
                } else {
                    cur = next_tok;
                }
                vec_free(exp);
                continue;
            } else if (cur->next && cur->next->kind == '(') {
                Vector *macro_args = vec_new();
                Vector *cur_arg = vec_new();
                int paren_depth = 0;
                cur = cur->next->next; /* skip ident and '(' */
                while (cur && cur->kind != TOK_EOF) {
                    if (cur->kind == '(') {
                        paren_depth++;
                        vec_push(cur_arg, clone_token(cur));
                    } else if (cur->kind == ')') {
                        if (paren_depth == 0) {
                            vec_push(macro_args, cur_arg);
                            cur = cur->next;
                            break;
                        }
                        paren_depth--;
                        vec_push(cur_arg, clone_token(cur));
                    } else if (cur->kind == ',' && paren_depth == 0) {
                        vec_push(macro_args, cur_arg);
                        cur_arg = vec_new();
                    } else {
                        vec_push(cur_arg, clone_token(cur));
                    }
                    cur = cur->next;
                }
                {
                    Vector *exp = expand_macro(cpp, m, macro_args);
                    Token *next_tok = cur;
                    if (exp->size > 0) {
                        for (k = 0; k < exp->size - 1; k++) {
                            ((Token *)vec_get(exp, k))->next = (Token *)vec_get(exp, k + 1);
                        }
                        ((Token *)vec_get(exp, exp->size - 1))->next = next_tok;
                        cur = (Token *)vec_get(exp, 0);
                    } else {
                        cur = next_tok;
                    }
                    vec_free(exp);
                }
                continue;
            }
        }
        vec_push(out, clone_token(cur));
        cur = cur->next;
    }
    return out;
}

static Vector *expand_macro(Preprocessor *cpp, Macro *m, Vector *args) {
    Vector *substituted = vec_new();
    Vector *result = vec_new();
    Vector **expanded_args = NULL;
    int i, j;

    if (m->is_function_like && args && args->size > 0) {
        expanded_args = (Vector **)c90_malloc(sizeof(Vector *) * args->size);
        for (j = 0; j < args->size; j++) {
            Vector *raw_arg = (Vector *)vec_get(args, j);
            expanded_args[j] = expand_token_vector(cpp, raw_arg);
        }
    }

    if (!m->is_function_like) {
        for (i = 0; i < m->body->size; i++) {
            Token *t = (Token *)vec_get(m->body, i);
            vec_push(substituted, clone_token(t));
        }
    } else {
        for (i = 0; i < m->body->size; i++) {
            Token *t = (Token *)vec_get(m->body, i);
            int is_param = 0;

            /* Stringification: #param */
            if (t->kind == '#' && i + 1 < m->body->size) {
                Token *next_t = (Token *)vec_get(m->body, i + 1);
                if (next_t->str && m->params) {
                    for (j = 0; j < m->params->size; j++) {
                        char *pname = (char *)vec_get(m->params, j);
                        if (strcmp(next_t->str, pname) == 0) {
                            StrBuf *sb = strbuf_new();
                            Token *str_tok;
                            if (args && j < args->size) {
                                Vector *arg_tokens = (Vector *)vec_get(args, j);
                                int k;
                                for (k = 0; k < arg_tokens->size; k++) {
                                    Token *at = (Token *)vec_get(arg_tokens, k);
                                    if (k > 0) strbuf_append_char(sb, ' ');
                                    if (at->str) strbuf_append_str(sb, at->str);
                                    else strbuf_append_str(sb, token_kind_str(at->kind));
                                }
                            }
                            str_tok = (Token *)c90_malloc(sizeof(Token));
                            str_tok->kind = TOK_STR_LIT;
                            str_tok->str = c90_strdup(sb->data);
                            str_tok->int_val = sb->length;
                            str_tok->is_unsigned = 0;
                            str_tok->filename = t->filename ? c90_strdup(t->filename) : NULL;
                            str_tok->line = t->line;
                            str_tok->col = t->col;
                            str_tok->hideset = NULL;
                            str_tok->next = NULL;
                            vec_push(substituted, str_tok);
                            strbuf_free(sb);
                            i++; /* skip next_t */
                            is_param = 1;
                            break;
                        }
                    }
                }
            }

            if (is_param) continue;

            if (t->str && m->params) {
                for (j = 0; j < m->params->size; j++) {
                    char *pname = (char *)vec_get(m->params, j);
                    if (strcmp(t->str, pname) == 0) {
                        int is_adjacent_to_hash_hash = 0;
                        is_param = 1;
                        if (i > 0 && ((Token *)vec_get(m->body, i - 1))->kind == TOK_HASH_HASH) {
                            is_adjacent_to_hash_hash = 1;
                        }
                        if (i + 1 < m->body->size && ((Token *)vec_get(m->body, i + 1))->kind == TOK_HASH_HASH) {
                            is_adjacent_to_hash_hash = 1;
                        }

                        if (args && j < args->size) {
                            Vector *arg_tokens = is_adjacent_to_hash_hash ? (Vector *)vec_get(args, j) : expanded_args[j];
                            int k;
                            if (arg_tokens) {
                                for (k = 0; k < arg_tokens->size; k++) {
                                    Token *at = (Token *)vec_get(arg_tokens, k);
                                    vec_push(substituted, clone_token(at));
                                }
                            }
                        }
                        break;
                    }
                }
            }

            if (!is_param) {
                vec_push(substituted, clone_token(t));
            }
        }
    }

    /* Process token pasting (## / TOK_HASH_HASH) */
    for (i = 0; i < substituted->size; i++) {
        Token *t = (Token *)vec_get(substituted, i);
        if (t->kind == TOK_HASH_HASH) {
            if (result->size > 0 && i + 1 < substituted->size) {
                Token *prev = (Token *)vec_pop(result);
                Token *next_t = (Token *)vec_get(substituted, ++i);
                char paste_buf[1024];
                Lexer *lex;
                Token *pasted_tok;

                sprintf(paste_buf, "%s%s", prev->str ? prev->str : token_kind_str(prev->kind),
                                           next_t->str ? next_t->str : token_kind_str(next_t->kind));
                lex = lexer_new(paste_buf, prev->filename);
                pasted_tok = lexer_tokenize(lex);
                if (pasted_tok && pasted_tok->kind != TOK_EOF) {
                    pasted_tok->filename = prev->filename ? c90_strdup(prev->filename) : NULL;
                    pasted_tok->line = prev->line;
                    pasted_tok->col = prev->col;
                    vec_push(result, pasted_tok);
                }
                free(lex);
            }
        } else {
            vec_push(result, t);
        }
    }

    /* Tag all tokens in result with m->name in their hideset */
    for (i = 0; i < result->size; i++) {
        Token *t = (Token *)vec_get(result, i);
        hideset_add(&t->hideset, m->name);
    }

    vec_free(substituted);
    return result;
}

/* Evaluate simple constant expression for #if / #elif */
static long eval_cpp_expr(Preprocessor *cpp, Token **pcur);

static long eval_cpp_primary(Preprocessor *cpp, Token **pcur) {
    Token *tok = *pcur;
    if (!tok || tok->kind == '\n' || tok->kind == TOK_EOF) return 0;

    if (tok->kind == TOK_INT_LIT || tok->kind == TOK_CHAR_LIT) {
        long val = tok->int_val;
        *pcur = tok->next;
        return val;
    }

    if (tok->kind == '(') {
        long val;
        *pcur = tok->next;
        val = eval_cpp_expr(cpp, pcur);
        if (*pcur && (*pcur)->kind == ')') {
            *pcur = (*pcur)->next;
        }
        return val;
    }

    if (tok->kind == '!') {
        *pcur = tok->next;
        return !eval_cpp_primary(cpp, pcur);
    }

    if (tok->kind == '~') {
        *pcur = tok->next;
        return ~eval_cpp_primary(cpp, pcur);
    }

    if (tok->kind == '-') {
        *pcur = tok->next;
        return -eval_cpp_primary(cpp, pcur);
    }

    if (tok->kind == '+') {
        *pcur = tok->next;
        return eval_cpp_primary(cpp, pcur);
    }

    if (tok->str) {
        if (strcmp(tok->str, "defined") == 0) {
            Token *next = tok->next;
            int has_paren = 0;
            int is_def = 0;
            if (next && next->kind == '(') {
                has_paren = 1;
                next = next->next;
            }
            if (next && next->str) {
                Macro *m = (Macro *)map_get(cpp->macros, next->str);
                is_def = (m != NULL);
                next = next->next;
            }
            if (has_paren && next && next->kind == ')') {
                next = next->next;
            }
            *pcur = next;
            return is_def;
        } else {
            Macro *m = NULL;
            if (!hideset_contains(tok->hideset, tok->str)) {
                m = (Macro *)map_get(cpp->macros, tok->str);
            }
            *pcur = tok->next;
            if (m && m->is_function_like && *pcur && (*pcur)->kind == '(') {
                Vector *args = vec_new();
                Vector *cur_arg = vec_new();
                int paren_depth = 0;
                *pcur = (*pcur)->next; /* skip '(' */
                while (*pcur) {
                    if ((*pcur)->kind == '(') {
                        paren_depth++;
                        vec_push(cur_arg, clone_token(*pcur));
                    } else if ((*pcur)->kind == ')') {
                        if (paren_depth == 0) {
                            vec_push(args, cur_arg);
                            *pcur = (*pcur)->next;
                            break;
                        }
                        paren_depth--;
                        vec_push(cur_arg, clone_token(*pcur));
                    } else if ((*pcur)->kind == ',' && paren_depth == 0) {
                        vec_push(args, cur_arg);
                        cur_arg = vec_new();
                    } else {
                        vec_push(cur_arg, clone_token(*pcur));
                    }
                    *pcur = (*pcur)->next;
                }
                {
                    Vector *exp = expand_macro(cpp, m, args);
                    if (exp->size > 0) {
                        Token *first = (Token *)vec_get(exp, 0);
                        int k;
                        for (k = 0; k < exp->size - 1; k++) {
                            ((Token *)vec_get(exp, k))->next = (Token *)vec_get(exp, k + 1);
                        }
                        ((Token *)vec_get(exp, exp->size - 1))->next = *pcur;
                        *pcur = first;
                        return eval_cpp_primary(cpp, pcur);
                    }
                }
            } else if (m && !m->is_function_like && m->body->size > 0) {
                Vector *exp = expand_macro(cpp, m, NULL);
                if (exp->size > 0) {
                    Token *first = (Token *)vec_get(exp, 0);
                    int k;
                    for (k = 0; k < exp->size - 1; k++) {
                        ((Token *)vec_get(exp, k))->next = (Token *)vec_get(exp, k + 1);
                    }
                    ((Token *)vec_get(exp, exp->size - 1))->next = *pcur;
                    *pcur = first;
                    return eval_cpp_primary(cpp, pcur);
                }
            }
            return 0;
        }
    }

    *pcur = tok->next;
    return 0;
}

static long eval_cpp_mul(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_primary(cpp, pcur);
    while (*pcur) {
        Token *op = *pcur;
        if (op->kind == '*') {
            *pcur = op->next;
            val = val * eval_cpp_primary(cpp, pcur);
        } else if (op->kind == '/') {
            long rhs;
            *pcur = op->next;
            rhs = eval_cpp_primary(cpp, pcur);
            val = rhs != 0 ? (val / rhs) : 0;
        } else if (op->kind == '%') {
            long rhs;
            *pcur = op->next;
            rhs = eval_cpp_primary(cpp, pcur);
            val = rhs != 0 ? (val % rhs) : 0;
        } else {
            break;
        }
    }
    return val;
}

static long eval_cpp_add(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_mul(cpp, pcur);
    while (*pcur) {
        Token *op = *pcur;
        if (op->kind == '+') {
            *pcur = op->next;
            val = val + eval_cpp_mul(cpp, pcur);
        } else if (op->kind == '-') {
            *pcur = op->next;
            val = val - eval_cpp_mul(cpp, pcur);
        } else {
            break;
        }
    }
    return val;
}

static long eval_cpp_shift(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_add(cpp, pcur);
    while (*pcur) {
        Token *op = *pcur;
        if (op->kind == TOK_SHL) {
            *pcur = op->next;
            val = val << eval_cpp_add(cpp, pcur);
        } else if (op->kind == TOK_SHR) {
            *pcur = op->next;
            val = val >> eval_cpp_add(cpp, pcur);
        } else {
            break;
        }
    }
    return val;
}

static long eval_cpp_rel(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_shift(cpp, pcur);
    while (*pcur) {
        Token *op = *pcur;
        if (op->kind == '<') {
            *pcur = op->next;
            val = val < eval_cpp_shift(cpp, pcur);
        } else if (op->kind == '>') {
            *pcur = op->next;
            val = val > eval_cpp_shift(cpp, pcur);
        } else if (op->kind == TOK_LE) {
            *pcur = op->next;
            val = val <= eval_cpp_shift(cpp, pcur);
        } else if (op->kind == TOK_GE) {
            *pcur = op->next;
            val = val >= eval_cpp_shift(cpp, pcur);
        } else {
            break;
        }
    }
    return val;
}

static long eval_cpp_eq(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_rel(cpp, pcur);
    while (*pcur) {
        Token *op = *pcur;
        if (op->kind == TOK_EQ) {
            *pcur = op->next;
            val = val == eval_cpp_rel(cpp, pcur);
        } else if (op->kind == TOK_NE) {
            *pcur = op->next;
            val = val != eval_cpp_rel(cpp, pcur);
        } else {
            break;
        }
    }
    return val;
}

static long eval_cpp_bitand(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_eq(cpp, pcur);
    while (*pcur && (*pcur)->kind == '&') {
        *pcur = (*pcur)->next;
        val = val & eval_cpp_eq(cpp, pcur);
    }
    return val;
}

static long eval_cpp_bitxor(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_bitand(cpp, pcur);
    while (*pcur && (*pcur)->kind == '^') {
        *pcur = (*pcur)->next;
        val = val ^ eval_cpp_bitand(cpp, pcur);
    }
    return val;
}

static long eval_cpp_bitor(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_bitxor(cpp, pcur);
    while (*pcur && (*pcur)->kind == '|') {
        *pcur = (*pcur)->next;
        val = val | eval_cpp_bitxor(cpp, pcur);
    }
    return val;
}

static long eval_cpp_land(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_bitor(cpp, pcur);
    while (*pcur && (*pcur)->kind == TOK_LAND) {
        *pcur = (*pcur)->next;
        val = eval_cpp_bitor(cpp, pcur) && val;
    }
    return val;
}

static long eval_cpp_lor(Preprocessor *cpp, Token **pcur) {
    long val = eval_cpp_land(cpp, pcur);
    while (*pcur && (*pcur)->kind == TOK_LOR) {
        *pcur = (*pcur)->next;
        val = eval_cpp_land(cpp, pcur) || val;
    }
    return val;
}

static long eval_cpp_cond(Preprocessor *cpp, Token **pcur) {
    long cond = eval_cpp_lor(cpp, pcur);
    if (*pcur && (*pcur)->kind == '?') {
        long then_val, else_val;
        *pcur = (*pcur)->next;
        then_val = eval_cpp_expr(cpp, pcur);
        if (*pcur && (*pcur)->kind == ':') {
            *pcur = (*pcur)->next;
        }
        else_val = eval_cpp_cond(cpp, pcur);
        return cond ? then_val : else_val;
    }
    return cond;
}

static long eval_cpp_expr(Preprocessor *cpp, Token **pcur) {
    return eval_cpp_cond(cpp, pcur);
}

/* ========================================================================= */
/* Preprocessor Driver and Directive Handler                                 */
/* ========================================================================= */

static int is_cond_active(Preprocessor *cpp) {
    int i;
    for (i = 0; i < cpp->cond_stack->size; i++) {
        CondState *cs = (CondState *)vec_get(cpp->cond_stack, i);
        if (!cs->active) return 0;
    }
    return 1;
}

static char *resolve_include_path(Preprocessor *cpp, const char *header_path, int is_angle, const char *cur_file) {
    int i;
    char path_buf[2048];
    FILE *fp;

    /* If quoted include, try current directory of including file first */
    if (!is_angle && cur_file) {
        char dir[1024];
        const char *last_slash = strrchr(cur_file, '/');
        if (last_slash) {
            int dlen = (int)(last_slash - cur_file);
            if (dlen >= 1024) dlen = 1023;
            strncpy(dir, cur_file, dlen);
            dir[dlen] = '\0';
            sprintf(path_buf, "%s/%s", dir, header_path);
        } else {
            sprintf(path_buf, "%s", header_path);
        }
        fp = fopen(path_buf, "r");
        if (fp) {
            fclose(fp);
            return c90_strdup(path_buf);
        }
    }

    /* Search configured include directories */
    for (i = 0; i < cpp->include_paths->size; i++) {
        char *inc_dir = (char *)vec_get(cpp->include_paths, i);
        sprintf(path_buf, "%s/%s", inc_dir, header_path);
        fp = fopen(path_buf, "r");
        if (fp) {
            fclose(fp);
            return c90_strdup(path_buf);
        }
    }

    return NULL;
}

Token *cpp_process(Preprocessor *cpp, const char *source, const char *filename) {
    Lexer *l = lexer_new(source, filename);
    Token *toks = lexer_tokenize(l);
    Token head;
    Token *tail = &head;
    Token *cur = toks;

    head.next = NULL;

    while (cur && cur->kind != TOK_EOF) {
        /* Check for preprocessor directive indicated by '#' token */
        if (cur->kind == '#') {
            Token *hash_tok = cur;
            Token *directive = cur->next;
            int dir_line = hash_tok->line;

            if (!directive || directive->line != dir_line) {
                /* Null directive, skip */
                cur = directive;
                continue;
            }

            if (directive->kind == TOK_IF || (directive->kind == TOK_IDENT && strcmp(directive->str, "if") == 0)) {
                int parent_act = is_cond_active(cpp);
                long cond_val = 0;
                CondState *cs = (CondState *)c90_malloc(sizeof(CondState));
                cur = directive->next;
                if (parent_act) {
                    cond_val = eval_cpp_expr(cpp, &cur);
                    while (cur && cur->line == dir_line) cur = cur->next;
                } else {
                    while (cur && cur->line == dir_line) cur = cur->next;
                }
                cs->parent_active = parent_act;
                cs->active = parent_act && (cond_val != 0);
                cs->branch_taken = cs->active;
                vec_push(cpp->cond_stack, cs);
                continue;
            } else if (directive->kind == TOK_IDENT && strcmp(directive->str, "ifdef") == 0) {
                int parent_act = is_cond_active(cpp);
                Token *id_tok = directive->next;
                CondState *cs = (CondState *)c90_malloc(sizeof(CondState));
                int is_def = 0;
                if (id_tok && id_tok->str) {
                    Macro *m = (Macro *)map_get(cpp->macros, id_tok->str);
                    is_def = (m != NULL);
                    cur = id_tok->next;
                } else {
                    cur = directive->next;
                }
                while (cur && cur->line == dir_line) cur = cur->next;
                cs->parent_active = parent_act;
                cs->active = parent_act && is_def;
                cs->branch_taken = cs->active;
                vec_push(cpp->cond_stack, cs);
                continue;
            } else if (directive->kind == TOK_IDENT && strcmp(directive->str, "ifndef") == 0) {
                int parent_act = is_cond_active(cpp);
                Token *id_tok = directive->next;
                CondState *cs = (CondState *)c90_malloc(sizeof(CondState));
                int is_ndef = 0;
                if (id_tok && id_tok->str) {
                    Macro *m = (Macro *)map_get(cpp->macros, id_tok->str);
                    is_ndef = (m == NULL);
                    cur = id_tok->next;
                } else {
                    cur = directive->next;
                }
                while (cur && cur->line == dir_line) cur = cur->next;
                cs->parent_active = parent_act;
                cs->active = parent_act && is_ndef;
                cs->branch_taken = cs->active;
                vec_push(cpp->cond_stack, cs);
                continue;
            } else if (directive->kind == TOK_IDENT && (strcmp(directive->str, "elif") == 0 || directive->kind == TOK_ELSE)) {
                if (cpp->cond_stack->size == 0) {
                    c90_error(filename, dir_line, "#elif without #if");
                } else {
                    CondState *cs = (CondState *)vec_get(cpp->cond_stack, cpp->cond_stack->size - 1);
                    cur = directive->next;
                    if (cs->parent_active && !cs->branch_taken) {
                        long cond_val = eval_cpp_expr(cpp, &cur);
                        while (cur && cur->line == dir_line) cur = cur->next;
                        if (cond_val != 0) {
                            cs->active = 1;
                            cs->branch_taken = 1;
                        } else {
                            cs->active = 0;
                        }
                    } else {
                        cs->active = 0;
                        while (cur && cur->line == dir_line) cur = cur->next;
                    }
                }
                continue;
            } else if (directive->kind == TOK_ELSE || (directive->kind == TOK_IDENT && strcmp(directive->str, "else") == 0)) {
                if (cpp->cond_stack->size == 0) {
                    c90_error(filename, dir_line, "#else without #if");
                } else {
                    CondState *cs = (CondState *)vec_get(cpp->cond_stack, cpp->cond_stack->size - 1);
                    if (cs->parent_active && !cs->branch_taken) {
                        cs->active = 1;
                        cs->branch_taken = 1;
                    } else {
                        cs->active = 0;
                    }
                }
                cur = directive->next;
                while (cur && cur->line == dir_line) cur = cur->next;
                continue;
            } else if (directive->kind == TOK_IDENT && strcmp(directive->str, "endif") == 0) {
                if (cpp->cond_stack->size == 0) {
                    c90_error(filename, dir_line, "#endif without #if");
                } else {
                    CondState *cs = (CondState *)vec_pop(cpp->cond_stack);
                    free(cs);
                }
                cur = directive->next;
                while (cur && cur->line == dir_line) cur = cur->next;
                continue;
            }

            /* If current conditional block is inactive, ignore remaining directive */
            if (!is_cond_active(cpp)) {
                cur = directive->next;
                while (cur && cur->line == dir_line) cur = cur->next;
                continue;
            }

            if (directive->kind == TOK_IDENT && strcmp(directive->str, "define") == 0) {
                Token *name_tok = directive->next;
                Macro *m;
                if (!name_tok || !name_tok->str) {
                    c90_error(filename, dir_line, "#define requires macro name");
                }
                m = (Macro *)c90_malloc(sizeof(Macro));
                m->name = c90_strdup(name_tok->str);
                m->is_function_like = 0;
                m->params = NULL;
                m->body = vec_new();
                m->is_builtin = 0;

                cur = name_tok->next;
                /* Check for function-like macro: #define FOO(a, b) */
                if (cur && cur->kind == '(' && cur->col == name_tok->col + (int)strlen(name_tok->str)) {
                    m->is_function_like = 1;
                    m->params = vec_new();
                    cur = cur->next;
                    while (cur && cur->kind != ')' && cur->line == dir_line) {
                        if (cur->str) {
                            vec_push(m->params, c90_strdup(cur->str));
                        }
                        cur = cur->next;
                        if (cur && cur->kind == ',') cur = cur->next;
                    }
                    if (cur && cur->kind == ')') cur = cur->next;
                }

                while (cur && cur->line == dir_line) {
                    vec_push(m->body, clone_token(cur));
                    cur = cur->next;
                }
                map_put(cpp->macros, m->name, m);
                continue;
            } else if (directive->kind == TOK_IDENT && strcmp(directive->str, "undef") == 0) {
                Token *name_tok = directive->next;
                if (name_tok && name_tok->str) {
                    map_put(cpp->macros, name_tok->str, NULL);
                    cur = name_tok->next;
                } else {
                    cur = directive->next;
                }
                while (cur && cur->line == dir_line) cur = cur->next;
                continue;
            } else if (directive->kind == TOK_IDENT && strcmp(directive->str, "include") == 0) {
                Token *inc_tok = directive->next;
                char header_path[512] = "";
                int is_angle = 0;
                char *resolved;
                char *hdr_content;

                if (inc_tok && inc_tok->kind == TOK_STR_LIT) {
                    strcpy(header_path, inc_tok->str);
                    cur = inc_tok->next;
                } else if (inc_tok && inc_tok->kind == '<') {
                    is_angle = 1;
                    cur = inc_tok->next;
                    while (cur && cur->kind != '>' && cur->line == dir_line) {
                        if (cur->str) strcat(header_path, cur->str);
                        cur = cur->next;
                    }
                    if (cur && cur->kind == '>') cur = cur->next;
                }

                while (cur && cur->line == dir_line) cur = cur->next;

                resolved = resolve_include_path(cpp, header_path, is_angle, filename);
                if (!resolved) {
                    c90_error(filename, dir_line, "cannot find include header '%s'", header_path);
                }

                hdr_content = read_entire_file(resolved);
                if (!hdr_content) {
                    c90_error(filename, dir_line, "failed to read include header '%s'", resolved);
                }

                {
                    Token *inc_tokens = cpp_process(cpp, hdr_content, resolved);
                    Token *p = inc_tokens;
                    while (p && p->kind != TOK_EOF) {
                        tail->next = clone_token(p);
                        tail = tail->next;
                        p = p->next;
                    }
                    free(hdr_content);
                    free(resolved);
                }
                continue;
            } else {
                /* Unknown directive (e.g. #pragma), skip line */
                cur = directive->next;
                while (cur && cur->line == dir_line) cur = cur->next;
                continue;
            }
        }

        /* If current condition branch is active, emit or expand token */
        if (is_cond_active(cpp)) {
            Macro *m = NULL;
            if (cur->kind == TOK_IDENT) {
                /* Built-in macros */
                if (strcmp(cur->str, "__FILE__") == 0) {
                    Token *tok = clone_token(cur);
                    tok->kind = TOK_STR_LIT;
                    tok->str = c90_strdup(cur->filename);
                    tail->next = tok;
                    tail = tail->next;
                    cur = cur->next;
                    continue;
                } else if (strcmp(cur->str, "__LINE__") == 0) {
                    Token *tok = clone_token(cur);
                    tok->kind = TOK_INT_LIT;
                    tok->int_val = cur->line;
                    tail->next = tok;
                    tail = tail->next;
                    cur = cur->next;
                    continue;
                }
                if (!hideset_contains(cur->hideset, cur->str)) {
                    m = (Macro *)map_get(cpp->macros, cur->str);
                }
            }

            if (m) {
                if (!m->is_function_like) {
                    Vector *expanded = expand_macro(cpp, m, NULL);
                    Token *next_tok = cur->next;
                    if (expanded->size == 1) {
                        Token *only = (Token *)vec_get(expanded, 0);
                        if (only->str && strcmp(only->str, m->name) == 0) {
                            tail->next = only;
                            tail = tail->next;
                            cur = next_tok;
                            vec_free(expanded);
                            continue;
                        }
                    }
                    if (expanded->size > 0) {
                        int k;
                        for (k = 0; k < expanded->size - 1; k++) {
                            Token *t1 = (Token *)vec_get(expanded, k);
                            Token *t2 = (Token *)vec_get(expanded, k + 1);
                            t1->next = t2;
                        }
                        ((Token *)vec_get(expanded, expanded->size - 1))->next = next_tok;
                        cur = (Token *)vec_get(expanded, 0);
                    } else {
                        cur = next_tok;
                    }
                    vec_free(expanded);
                    continue;
                } else if (cur->next && cur->next->kind == '(') {
                    /* Function-like macro invocation */
                    Vector *args = vec_new();
                    Vector *cur_arg = vec_new();
                    int paren_depth = 0;
                    cur = cur->next->next; /* skip ident and '(' */

                    while (cur && cur->kind != TOK_EOF) {
                        if (cur->kind == '(') {
                            paren_depth++;
                            vec_push(cur_arg, clone_token(cur));
                        } else if (cur->kind == ')') {
                            if (paren_depth == 0) {
                                vec_push(args, cur_arg);
                                cur = cur->next;
                                break;
                            }
                            paren_depth--;
                            vec_push(cur_arg, clone_token(cur));
                        } else if (cur->kind == ',' && paren_depth == 0) {
                            vec_push(args, cur_arg);
                            cur_arg = vec_new();
                        } else {
                            vec_push(cur_arg, clone_token(cur));
                        }
                        cur = cur->next;
                    }

                    {
                        Vector *expanded = expand_macro(cpp, m, args);
                        Token *next_tok = cur;
                        if (expanded->size > 0) {
                            int k;
                            for (k = 0; k < expanded->size - 1; k++) {
                                Token *t1 = (Token *)vec_get(expanded, k);
                                Token *t2 = (Token *)vec_get(expanded, k + 1);
                                t1->next = t2;
                            }
                            ((Token *)vec_get(expanded, expanded->size - 1))->next = next_tok;
                            cur = (Token *)vec_get(expanded, 0);
                        }
                        vec_free(expanded);
                    }
                    continue;
                }
            }

            tail->next = clone_token(cur);
            tail = tail->next;
        }

        cur = cur->next;
    }

    tail->next = (Token *)c90_malloc(sizeof(Token));
    tail->next->kind = TOK_EOF;
    tail->next->str = NULL;
    tail->next->filename = filename;
    tail->next->line = 0;
    tail->next->col = 0;
    tail->next->next = NULL;

    free(l);
    return head.next;
}
