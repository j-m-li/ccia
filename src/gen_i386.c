/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include "../include/c90.h"

static void gen_expr(CodeGen *gen, AstNode *node);
static void gen_lval(CodeGen *gen, AstNode *node);
static void gen_stmt(CodeGen *gen, AstNode *node);

CodeGen *codegen_new(FILE *out, AstNode *root) {
    CodeGen *gen = (CodeGen *)c90_malloc(sizeof(CodeGen));
    gen->out = out;
    gen->root = root;
    gen->label_count = 1;
    gen->func_ret_label = NULL;
    gen->break_stack = vec_new();
    gen->continue_stack = vec_new();
    return gen;
}

static char *gen_asm_label(CodeGen *gen, const char *prefix) {
    char buf[64];
    sprintf(buf, ".L_%s_%d", prefix, gen->label_count++);
    return c90_strdup(buf);
}

static void emit(CodeGen *gen, const char *fmt, ...) {
    const char *a1;
    const char *a2;
    const char *p = fmt;
    int arg_idx = 0;
    int is_long = 0;
#ifdef __CC90__
#ifdef TARGET_I386
    a1 = *((const char **)&fmt + 1);
    a2 = *((const char **)&fmt + 2);
#else
    a1 = *((const char **)&fmt - 1);
    a2 = *((const char **)&fmt - 2);
#endif
#else
    va_list ap;
    va_start(ap, fmt);
    a1 = va_arg(ap, char *);
    a2 = va_arg(ap, char *);
    va_end(ap);
#endif

    while (*p) {
        if (*p == '%' && *(p + 1) == '%') {
            fputc('%', gen->out);
            p += 2;
        } else if (*p == '%') {
            p++;
            is_long = 0;
            if (*p == 'l') { is_long = 1; p++; }
            if (*p == 's') {
                const char *s = (arg_idx == 0) ? a1 : a2;
                if (s) fputs(s, gen->out);
                arg_idx++;
                p++;
            } else if (*p == 'd' || *p == 'i') {
                if (is_long) {
                    long val = (arg_idx == 0) ? (long)a1 : (long)a2;
                    fprintf(gen->out, "%ld", val);
                } else {
                    int val = (arg_idx == 0) ? (int)(long)a1 : (int)(long)a2;
                    fprintf(gen->out, "%d", val);
                }
                arg_idx++;
                p++;
            } else if (*p == 'u') {
                if (is_long) {
                    unsigned long val = (arg_idx == 0) ? (unsigned long)a1 : (unsigned long)a2;
                    fprintf(gen->out, "%lu", val);
                } else {
                    unsigned int val = (arg_idx == 0) ? (unsigned int)(long)a1 : (unsigned int)(long)a2;
                    fprintf(gen->out, "%u", val);
                }
                arg_idx++;
                p++;
            } else if (*p == 'c') {
                int val = (arg_idx == 0) ? (int)(long)a1 : (int)(long)a2;
                fputc(val, gen->out);
                arg_idx++;
                p++;
            } else {
                fputc(*p, gen->out);
                p++;
            }
        } else {
            fputc(*p, gen->out);
            p++;
        }
    }
    fputc('\n', gen->out);
}

/* ========================================================================= */
/* Memory Load and Store Helper Functions (32-bit i386)                      */
/* ========================================================================= */

static void gen_load(CodeGen *gen, Type *type) {
    if (!type) {
        emit(gen, "    movl (%%eax), %%eax");
        return;
    }
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
        type->kind == TYPE_FUNC || type->kind == TYPE_LDOUBLE || type->kind == TYPE_DOUBLE) {
        /* Array/struct/func/ldouble/double evaluates to its base address */
        return;
    }
    if (type->size == 1) {
        if (type->is_unsigned) {
            emit(gen, "    movzbl (%%eax), %%eax");
        } else {
            emit(gen, "    movsbl (%%eax), %%eax");
        }
    } else if (type->size == 2) {
        if (type->is_unsigned) {
            emit(gen, "    movzwl (%%eax), %%eax");
        } else {
            emit(gen, "    movswl (%%eax), %%eax");
        }
    } else {
        emit(gen, "    movl (%%eax), %%eax");
    }
}

static void gen_store(CodeGen *gen, Type *type) {
    if (!type) {
        emit(gen, "    movl %%eax, (%%ecx)");
        return;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION || type->kind == TYPE_LDOUBLE || type->size > 4) {
        /* Copy multi-word struct/union/ldouble/double byte by byte */
        int i;
        for (i = 0; i < type->size; i++) {
            emit(gen, "    movb %d(%%eax), %%dl", i);
            emit(gen, "    movb %%dl, %d(%%ecx)", i);
        }
        return;
    }
    if (type->size == 1) {
        emit(gen, "    movb %%al, (%%ecx)");
    } else if (type->size == 2) {
        emit(gen, "    movw %%ax, (%%ecx)");
    } else {
        emit(gen, "    movl %%eax, (%%ecx)");
    }
}

/* ========================================================================= */
/* Lvalue Address Generation (32-bit i386)                                   */
/* ========================================================================= */

static void gen_lval(CodeGen *gen, AstNode *node) {
    if (!node) return;

    if (node->kind == AST_VAR) {
        Symbol *sym = node->u.sym;
        if (sym->is_global || sym->asm_label) {
            if (sym->asm_label) {
                emit(gen, "    movl $%s, %%eax", sym->asm_label);
            } else {
                emit(gen, "    movl $%s, %%eax", sym->name);
            }
        } else {
            emit(gen, "    leal %d(%%ebp), %%eax", sym->stack_offset);
        }
        return;
    }

    if (node->kind == AST_DEREF) {
        /* Address of *ptr is ptr itself */
        gen_expr(gen, node->u.unop.operand);
        return;
    }

    if (node->kind == AST_MEMBER) {
        if (node->u.member.is_arrow) {
            gen_expr(gen, node->u.member.target);
        } else {
            gen_lval(gen, node->u.member.target);
        }
        if (node->u.member.member && node->u.member.member->offset > 0) {
            emit(gen, "    addl $%d, %%eax", node->u.member.member->offset);
        }
        return;
    }

    c90_error(node->filename, node->line, "expression is not an lvalue");
}

/* ========================================================================= */
/* Expression Code Generation (32-bit i386)                                  */
/* ========================================================================= */

static int get_scratch_temp(CodeGen *gen) {
    int off = gen->scratch_base + (gen->ldouble_slot * 16);
    gen->ldouble_slot = (gen->ldouble_slot + 1) % 8;
    return off;
}

static void gen_cast_to(CodeGen *gen, Type *from, Type *to) {
    if (!from || !to || type_equal(from, to)) return;

    if (to->kind == TYPE_FLOAT) {
        if (from->kind == TYPE_DOUBLE) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __truncdfsf2");
            emit(gen, "    addl $4, %%esp");
        } else if (from->kind == TYPE_LDOUBLE) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __trunctfsf2");
            emit(gen, "    addl $4, %%esp");
        } else if (from->is_unsigned) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatunsisf");
            emit(gen, "    addl $4, %%esp");
        } else {
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatsisf");
            emit(gen, "    addl $4, %%esp");
        }
        return;
    }

    if (to->kind == TYPE_DOUBLE) {
        int off = get_scratch_temp(gen);
        if (from->kind == TYPE_FLOAT) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __extendsfdf2");
            emit(gen, "    addl $8, %%esp");
        } else if (from->kind == TYPE_LDOUBLE) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __trunctfdf2");
            emit(gen, "    addl $8, %%esp");
        } else if (from->is_unsigned) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatunsidf");
            emit(gen, "    addl $8, %%esp");
        } else {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatsidf");
            emit(gen, "    addl $8, %%esp");
        }
        emit(gen, "    leal -%d(%%ebp), %%eax", off);
        return;
    }

    if (to->kind == TYPE_LDOUBLE) {
        int off = get_scratch_temp(gen);
        if (from->kind == TYPE_FLOAT) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __extendsftf2");
            emit(gen, "    addl $8, %%esp");
        } else if (from->kind == TYPE_DOUBLE) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __extenddftf2");
            emit(gen, "    addl $8, %%esp");
        } else if (from->is_unsigned) {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatunsitf");
            emit(gen, "    addl $8, %%esp");
        } else {
            emit(gen, "    pushl %%eax");
            emit(gen, "    leal -%d(%%ebp), %%eax", off);
            emit(gen, "    pushl %%eax");
            emit(gen, "    call __floatsitf");
            emit(gen, "    addl $8, %%esp");
        }
        emit(gen, "    leal -%d(%%ebp), %%eax", off);
        return;
    }

    if (from->kind == TYPE_FLOAT) {
        emit(gen, "    pushl %%eax");
        if (to->is_unsigned) emit(gen, "    call __fixunssfsi");
        else emit(gen, "    call __fixsfsi");
        emit(gen, "    addl $4, %%esp");
        return;
    }

    if (from->kind == TYPE_DOUBLE) {
        emit(gen, "    pushl %%eax");
        if (to->is_unsigned) emit(gen, "    call __fixunsdfsi");
        else emit(gen, "    call __fixdfsi");
        emit(gen, "    addl $4, %%esp");
        return;
    }

    if (from->kind == TYPE_LDOUBLE) {
        emit(gen, "    pushl %%eax");
        if (to->is_unsigned) emit(gen, "    call __fixunstfsi");
        else emit(gen, "    call __fixtfsi");
        emit(gen, "    addl $4, %%esp");
        return;
    }

    /* Integer-to-integer conversion */
    if (to->size == 1) {
        if (to->is_unsigned) emit(gen, "    movzbl %%al, %%eax");
        else emit(gen, "    movsbl %%al, %%eax");
    } else if (to->size == 2) {
        if (to->is_unsigned) emit(gen, "    movzwl %%ax, %%eax");
        else emit(gen, "    movswl %%ax, %%eax");
    }
}

static void gen_expr(CodeGen *gen, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
        case AST_INT_LIT:
        case AST_CHAR_LIT:
            emit(gen, "    movl $%ld, %%eax", node->u.int_val.val);
            return;

        case AST_FLOAT_LIT:
            if (node->u.float_val.label) {
                if (node->type == type_float) {
                    emit(gen, "    movl %s, %%eax", node->u.float_val.label);
                } else {
                    emit(gen, "    leal %s, %%eax", node->u.float_val.label);
                }
            } else {
                emit(gen, "    movl $0, %%eax");
            }
            return;

        case AST_STR_LIT:
            emit(gen, "    movl $%s, %%eax", node->u.str_val.label);
            return;

        case AST_VAR:
            gen_lval(gen, node);
            gen_load(gen, node->type);
            return;

        case AST_ADDR:
            gen_lval(gen, node->u.unop.operand);
            return;

        case AST_DEREF:
            gen_expr(gen, node->u.unop.operand);
            gen_load(gen, node->type);
            return;

        case AST_POS:
            gen_expr(gen, node->u.unop.operand);
            return;

        case AST_NEG:
            gen_expr(gen, node->u.unop.operand);
            if (node->type && node->type->kind == TYPE_FLOAT) {
                emit(gen, "    xorl $0x80000000, %%eax");
            } else if (node->type && node->type->kind == TYPE_DOUBLE) {
                int off = get_scratch_temp(gen);
                emit(gen, "    pushl %%eax");
                emit(gen, "    leal -%d(%%ebp), %%eax", off);
                emit(gen, "    pushl %%eax");
                emit(gen, "    call __negdf2");
                emit(gen, "    addl $8, %%esp");
                emit(gen, "    leal -%d(%%ebp), %%eax", off);
            } else if (node->type && node->type->kind == TYPE_LDOUBLE) {
                int off = get_scratch_temp(gen);
                emit(gen, "    pushl %%eax");
                emit(gen, "    leal -%d(%%ebp), %%eax", off);
                emit(gen, "    pushl %%eax");
                emit(gen, "    call __negtf2");
                emit(gen, "    addl $8, %%esp");
                emit(gen, "    leal -%d(%%ebp), %%eax", off);
            } else {
                emit(gen, "    negl %%eax");
            }
            return;

        case AST_BITNOT:
            gen_expr(gen, node->u.unop.operand);
            emit(gen, "    notl %%eax");
            return;

        case AST_LOGNOT:
            gen_expr(gen, node->u.unop.operand);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    sete %%al");
            emit(gen, "    movzbl %%al, %%eax");
            return;

        case AST_CAST:
            gen_expr(gen, node->u.cast.operand);
            gen_cast_to(gen, node->u.cast.operand->type, node->type);
            return;

        case AST_PRE_INC:
        case AST_PRE_DEC: {
            int step = 1;
            if (node->type && node->type->base) step = node->type->base->size;
            gen_lval(gen, node->u.unop.operand);
            emit(gen, "    pushl %%eax");
            gen_load(gen, node->type);
            if (node->kind == AST_PRE_INC) {
                emit(gen, "    addl $%d, %%eax", step);
            } else {
                emit(gen, "    subl $%d, %%eax", step);
            }
            emit(gen, "    popl %%ecx");
            gen_store(gen, node->type);
            return;
        }

        case AST_POST_INC:
        case AST_POST_DEC: {
            int step = 1;
            if (node->type && node->type->base) step = node->type->base->size;
            gen_lval(gen, node->u.unop.operand);
            emit(gen, "    pushl %%eax");
            gen_load(gen, node->type);
            emit(gen, "    pushl %%eax"); /* Save original value */
            if (node->kind == AST_POST_INC) {
                emit(gen, "    addl $%d, %%eax", step);
            } else {
                emit(gen, "    subl $%d, %%eax", step);
            }
            emit(gen, "    movl 4(%%esp), %%ecx");
            gen_store(gen, node->type);
            emit(gen, "    popl %%eax"); /* Restore original value */
            emit(gen, "    addl $4, %%esp"); /* Pop saved lval */
            return;
        }

        case AST_MEMBER:
            gen_lval(gen, node);
            gen_load(gen, node->type);
            return;

        case AST_ASSIGN:
            gen_lval(gen, node->u.binop.lhs);
            emit(gen, "    pushl %%eax");
            gen_expr(gen, node->u.binop.rhs);
            if (node->u.binop.lhs->type && node->u.binop.rhs->type &&
                !type_equal(node->u.binop.lhs->type, node->u.binop.rhs->type)) {
                gen_cast_to(gen, node->u.binop.rhs->type, node->u.binop.lhs->type);
            }
            emit(gen, "    popl %%ecx");
            gen_store(gen, node->type);
            return;

        case AST_ADD_ASSIGN:
        case AST_SUB_ASSIGN:
        case AST_MUL_ASSIGN:
        case AST_DIV_ASSIGN:
        case AST_MOD_ASSIGN:
        case AST_SHL_ASSIGN:
        case AST_SHR_ASSIGN:
        case AST_AND_ASSIGN:
        case AST_XOR_ASSIGN:
        case AST_OR_ASSIGN: {
            int step = 1;
            int is_unsigned = node->type && node->type->is_unsigned;
            if (node->type && node->type->base) step = node->type->base->size;
            gen_lval(gen, node->u.binop.lhs);
            emit(gen, "    pushl %%eax");
            gen_load(gen, node->type);
            emit(gen, "    pushl %%eax");
            gen_expr(gen, node->u.binop.rhs);
            emit(gen, "    movl %%eax, %%ecx");
            emit(gen, "    popl %%eax");

            if (node->kind == AST_ADD_ASSIGN) {
                if (step > 1) emit(gen, "    imull $%d, %%ecx", step);
                emit(gen, "    addl %%ecx, %%eax");
            } else if (node->kind == AST_SUB_ASSIGN) {
                if (step > 1) emit(gen, "    imull $%d, %%ecx", step);
                emit(gen, "    subl %%ecx, %%eax");
            } else if (node->kind == AST_MUL_ASSIGN) {
                emit(gen, "    imull %%ecx, %%eax");
            } else if (node->kind == AST_DIV_ASSIGN) {
                if (is_unsigned) {
                    emit(gen, "    xorl %%edx, %%edx");
                    emit(gen, "    divl %%ecx");
                } else {
                    emit(gen, "    cltd");
                    emit(gen, "    idivl %%ecx");
                }
            } else if (node->kind == AST_MOD_ASSIGN) {
                if (is_unsigned) {
                    emit(gen, "    xorl %%edx, %%edx");
                    emit(gen, "    divl %%ecx");
                    emit(gen, "    movl %%edx, %%eax");
                } else {
                    emit(gen, "    cltd");
                    emit(gen, "    idivl %%ecx");
                    emit(gen, "    movl %%edx, %%eax");
                }
            } else if (node->kind == AST_SHL_ASSIGN) {
                emit(gen, "    shll %%cl, %%eax");
            } else if (node->kind == AST_SHR_ASSIGN) {
                if (is_unsigned) {
                    emit(gen, "    shrl %%cl, %%eax");
                } else {
                    emit(gen, "    sarl %%cl, %%eax");
                }
            } else if (node->kind == AST_AND_ASSIGN) {
                emit(gen, "    andl %%ecx, %%eax");
            } else if (node->kind == AST_XOR_ASSIGN) {
                emit(gen, "    xorl %%ecx, %%eax");
            } else if (node->kind == AST_OR_ASSIGN) {
                emit(gen, "    orl %%ecx, %%eax");
            }

            emit(gen, "    popl %%ecx");
            gen_store(gen, node->type);
            return;
        }

        case AST_COMMA:
            gen_expr(gen, node->u.binop.lhs);
            gen_expr(gen, node->u.binop.rhs);
            return;

        case AST_LOGAND: {
            char *label_false = gen_asm_label(gen, "land_false");
            char *label_end = gen_asm_label(gen, "land_end");
            gen_expr(gen, node->u.binop.lhs);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    je %s", label_false);
            gen_expr(gen, node->u.binop.rhs);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    je %s", label_false);
            emit(gen, "    movl $1, %%eax");
            emit(gen, "    jmp %s", label_end);
            emit(gen, "%s:", label_false);
            emit(gen, "    movl $0, %%eax");
            emit(gen, "%s:", label_end);
            return;
        }

        case AST_LOGOR: {
            char *label_true = gen_asm_label(gen, "lor_true");
            char *label_end = gen_asm_label(gen, "lor_end");
            gen_expr(gen, node->u.binop.lhs);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    jne %s", label_true);
            gen_expr(gen, node->u.binop.rhs);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    jne %s", label_true);
            emit(gen, "    movl $0, %%eax");
            emit(gen, "    jmp %s", label_end);
            emit(gen, "%s:", label_true);
            emit(gen, "    movl $1, %%eax");
            emit(gen, "%s:", label_end);
            return;
        }

        case AST_COND: {
            char *label_else = gen_asm_label(gen, "cond_else");
            char *label_end = gen_asm_label(gen, "cond_end");
            gen_expr(gen, node->u.cond.cond);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    je %s", label_else);
            gen_expr(gen, node->u.cond.then_expr);
            emit(gen, "    jmp %s", label_end);
            emit(gen, "%s:", label_else);
            gen_expr(gen, node->u.cond.else_expr);
            emit(gen, "%s:", label_end);
            return;
        }

        case AST_CALL: {
            Vector *args = node->u.call.args;
            int num_args = args ? args->size : 0;
            int i;

            if (node->u.call.func->kind == AST_VAR && node->u.call.func->u.sym->kind == SYM_FUNC) {
                const char *fname = node->u.call.func->u.sym->name;
                if (strcmp(fname, "__builtin_bswap16") == 0) {
                    if (node->u.call.args && node->u.call.args->size > 0) {
                        gen_expr(gen, (AstNode *)vec_get(node->u.call.args, 0));
                        emit(gen, "    rolw $8, %%ax");
                        emit(gen, "    movzwl %%ax, %%eax");
                    }
                    return;
                }
                if (strcmp(fname, "__builtin_bswap32") == 0) {
                    if (node->u.call.args && node->u.call.args->size > 0) {
                        gen_expr(gen, (AstNode *)vec_get(node->u.call.args, 0));
                        emit(gen, "    bswapl %%eax");
                    }
                    return;
                }
                if (strcmp(fname, "__builtin_bswap64") == 0) {
                    if (node->u.call.args && node->u.call.args->size > 0) {
                        gen_expr(gen, (AstNode *)vec_get(node->u.call.args, 0));
                        emit(gen, "    bswapl %%eax");
                    }
                    return;
                }
                if (strcmp(fname, "__builtin_expect") == 0) {
                    if (node->u.call.args && node->u.call.args->size > 0) {
                        gen_expr(gen, (AstNode *)vec_get(node->u.call.args, 0));
                    }
                    return;
                }
                if (strcmp(fname, "__builtin_constant_p") == 0) {
                    emit(gen, "    movl $0, %%eax");
                    return;
                }
                if (strcmp(fname, "__builtin_va_start") == 0) {
                    if (node->u.call.args && node->u.call.args->size > 0) {
                        gen_lval(gen, (AstNode *)vec_get(node->u.call.args, 0));
                        emit(gen, "    pushl %%eax");
                        emit(gen, "    leal 12(%%ebp), %%eax");
                        emit(gen, "    popl %%ecx");
                        emit(gen, "    movl %%eax, (%%ecx)");
                    }
                    return;
                }
                if (strcmp(fname, "__builtin_va_end") == 0) {
                    return;
                }
            }

            /* Push arguments in reverse order (right to left) for cdecl */
            for (i = num_args - 1; i >= 0; i--) {
                AstNode *arg = (AstNode *)vec_get(args, i);
                gen_expr(gen, arg);
                emit(gen, "    pushl %%eax");
            }

            if (node->u.call.func->kind == AST_VAR && node->u.call.func->u.sym->kind == SYM_FUNC) {
                emit(gen, "    call %s", node->u.call.func->u.sym->name);
            } else {
                gen_expr(gen, node->u.call.func);
                emit(gen, "    call *%%eax");
            }

            /* Caller cleans up stack in cdecl */
            if (num_args > 0) {
                emit(gen, "    addl $%d, %%esp", num_args * 4);
            }
            return;
        }

        default:
            break;
    }

    /* Binary Arithmetic / Relational Operations */
    {
        AstNode *lhs = node->u.binop.lhs;
        AstNode *rhs = node->u.binop.rhs;
        Type *common_type = type_max(lhs->type, rhs->type);
        int is_ptr_lhs, is_ptr_rhs, is_unsigned;

        if (common_type && type_is_floating(common_type)) {
            gen_expr(gen, lhs);
            if (!type_equal(lhs->type, common_type)) {
                gen_cast_to(gen, lhs->type, common_type);
            }
            emit(gen, "    pushl %%eax");
            gen_expr(gen, rhs);
            if (!type_equal(rhs->type, common_type)) {
                gen_cast_to(gen, rhs->type, common_type);
            }
            emit(gen, "    movl %%eax, %%ecx"); /* rhs */
            emit(gen, "    popl %%eax");        /* lhs */

            if (common_type->kind == TYPE_FLOAT) {
                emit(gen, "    pushl %%ecx");
                emit(gen, "    pushl %%eax");
                switch (node->kind) {
                    case AST_ADD: emit(gen, "    call __addsf3"); break;
                    case AST_SUB: emit(gen, "    call __subsf3"); break;
                    case AST_MUL: emit(gen, "    call __mulsf3"); break;
                    case AST_DIV: emit(gen, "    call __divsf3"); break;
                    case AST_EQ: emit(gen, "    call __eqsf2"); emit(gen, "    testl %%eax, %%eax; sete %%al; movzbl %%al, %%eax"); break;
                    case AST_NE: emit(gen, "    call __nesf2"); emit(gen, "    testl %%eax, %%eax; setne %%al; movzbl %%al, %%eax"); break;
                    case AST_LT: emit(gen, "    call __ltsf2"); emit(gen, "    testl %%eax, %%eax; setl %%al; movzbl %%al, %%eax"); break;
                    case AST_LE: emit(gen, "    call __lesf2"); emit(gen, "    testl %%eax, %%eax; setle %%al; movzbl %%al, %%eax"); break;
                    case AST_GT: emit(gen, "    call __gtsf2"); emit(gen, "    testl %%eax, %%eax; setg %%al; movzbl %%al, %%eax"); break;
                    case AST_GE: emit(gen, "    call __gesf2"); emit(gen, "    testl %%eax, %%eax; setge %%al; movzbl %%al, %%eax"); break;
                    default: break;
                }
                emit(gen, "    addl $8, %%esp");
            } else if (common_type->kind == TYPE_DOUBLE) {
                if (node->kind == AST_EQ || node->kind == AST_NE || node->kind == AST_LT ||
                    node->kind == AST_LE || node->kind == AST_GT || node->kind == AST_GE) {
                    emit(gen, "    pushl %%ecx");
                    emit(gen, "    pushl %%eax");
                    switch (node->kind) {
                        case AST_EQ: emit(gen, "    call __eqdf2"); emit(gen, "    testl %%eax, %%eax; sete %%al; movzbl %%al, %%eax"); break;
                        case AST_NE: emit(gen, "    call __nedf2"); emit(gen, "    testl %%eax, %%eax; setne %%al; movzbl %%al, %%eax"); break;
                        case AST_LT: emit(gen, "    call __ltdf2"); emit(gen, "    testl %%eax, %%eax; setl %%al; movzbl %%al, %%eax"); break;
                        case AST_LE: emit(gen, "    call __ledf2"); emit(gen, "    testl %%eax, %%eax; setle %%al; movzbl %%al, %%eax"); break;
                        case AST_GT: emit(gen, "    call __gtdf2"); emit(gen, "    testl %%eax, %%eax; setg %%al; movzbl %%al, %%eax"); break;
                        case AST_GE: emit(gen, "    call __gedf2"); emit(gen, "    testl %%eax, %%eax; setge %%al; movzbl %%al, %%eax"); break;
                        default: break;
                    }
                    emit(gen, "    addl $8, %%esp");
                } else {
                    int off = get_scratch_temp(gen);
                    emit(gen, "    pushl %%ecx"); /* rhs */
                    emit(gen, "    pushl %%eax"); /* lhs */
                    emit(gen, "    leal -%d(%%ebp), %%eax", off);
                    emit(gen, "    pushl %%eax"); /* res */
                    switch (node->kind) {
                        case AST_ADD: emit(gen, "    call __adddf3"); break;
                        case AST_SUB: emit(gen, "    call __subdf3"); break;
                        case AST_MUL: emit(gen, "    call __muldf3"); break;
                        case AST_DIV: emit(gen, "    call __divdf3"); break;
                        default: break;
                    }
                    emit(gen, "    addl $12, %%esp");
                    emit(gen, "    leal -%d(%%ebp), %%eax", off);
                }
            } else if (common_type->kind == TYPE_LDOUBLE) {
                if (node->kind == AST_EQ || node->kind == AST_NE || node->kind == AST_LT ||
                    node->kind == AST_LE || node->kind == AST_GT || node->kind == AST_GE) {
                    emit(gen, "    pushl %%ecx");
                    emit(gen, "    pushl %%eax");
                    switch (node->kind) {
                        case AST_EQ: emit(gen, "    call __eqtf2"); emit(gen, "    testl %%eax, %%eax; sete %%al; movzbl %%al, %%eax"); break;
                        case AST_NE: emit(gen, "    call __netf2"); emit(gen, "    testl %%eax, %%eax; setne %%al; movzbl %%al, %%eax"); break;
                        case AST_LT: emit(gen, "    call __lttf2"); emit(gen, "    testl %%eax, %%eax; setl %%al; movzbl %%al, %%eax"); break;
                        case AST_LE: emit(gen, "    call __letf2"); emit(gen, "    testl %%eax, %%eax; setle %%al; movzbl %%al, %%eax"); break;
                        case AST_GT: emit(gen, "    call __gttf2"); emit(gen, "    testl %%eax, %%eax; setg %%al; movzbl %%al, %%eax"); break;
                        case AST_GE: emit(gen, "    call __getf2"); emit(gen, "    testl %%eax, %%eax; setge %%al; movzbl %%al, %%eax"); break;
                        default: break;
                    }
                    emit(gen, "    addl $8, %%esp");
                } else {
                    int off = get_scratch_temp(gen);
                    emit(gen, "    pushl %%ecx"); /* rhs */
                    emit(gen, "    pushl %%eax"); /* lhs */
                    emit(gen, "    leal -%d(%%ebp), %%eax", off);
                    emit(gen, "    pushl %%eax"); /* res */
                    switch (node->kind) {
                        case AST_ADD: emit(gen, "    call __addtf3"); break;
                        case AST_SUB: emit(gen, "    call __subtf3"); break;
                        case AST_MUL: emit(gen, "    call __multf3"); break;
                        case AST_DIV: emit(gen, "    call __divtf3"); break;
                        default: break;
                    }
                    emit(gen, "    addl $12, %%esp");
                    emit(gen, "    leal -%d(%%ebp), %%eax", off);
                }
            }
            return;
        }

        is_ptr_lhs = type_is_pointer(lhs->type);
        is_ptr_rhs = type_is_pointer(rhs->type);
        is_unsigned = (node->type && node->type->is_unsigned) ||
                      (lhs->type && lhs->type->is_unsigned) ||
                      (rhs->type && rhs->type->is_unsigned);

        gen_expr(gen, lhs);
        emit(gen, "    pushl %%eax");
        gen_expr(gen, rhs);
        emit(gen, "    movl %%eax, %%ecx");
        emit(gen, "    popl %%eax");

        switch (node->kind) {
            case AST_ADD:
                if (is_ptr_lhs && !is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    if (scale > 1) emit(gen, "    imull $%d, %%ecx", scale);
                } else if (!is_ptr_lhs && is_ptr_rhs) {
                    int scale = rhs->type->base ? rhs->type->base->size : 1;
                    if (scale > 1) emit(gen, "    imull $%d, %%eax", scale);
                }
                emit(gen, "    addl %%ecx, %%eax");
                break;

            case AST_SUB:
                if (is_ptr_lhs && !is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    if (scale > 1) emit(gen, "    imull $%d, %%ecx", scale);
                    emit(gen, "    subl %%ecx, %%eax");
                } else if (is_ptr_lhs && is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    emit(gen, "    subl %%ecx, %%eax");
                    if (scale > 1) {
                        emit(gen, "    movl $%d, %%ecx", scale);
                        emit(gen, "    cltd");
                        emit(gen, "    idivl %%ecx");
                    }
                } else {
                    emit(gen, "    subl %%ecx, %%eax");
                }
                break;

            case AST_MUL:
                emit(gen, "    imull %%ecx, %%eax");
                break;

            case AST_DIV:
                if (is_unsigned) {
                    emit(gen, "    xorl %%edx, %%edx");
                    emit(gen, "    divl %%ecx");
                } else {
                    emit(gen, "    cltd");
                    emit(gen, "    idivl %%ecx");
                }
                break;

            case AST_MOD:
                if (is_unsigned) {
                    emit(gen, "    xorl %%edx, %%edx");
                    emit(gen, "    divl %%ecx");
                    emit(gen, "    movl %%edx, %%eax");
                } else {
                    emit(gen, "    cltd");
                    emit(gen, "    idivl %%ecx");
                    emit(gen, "    movl %%edx, %%eax");
                }
                break;

            case AST_SHL:
                emit(gen, "    shll %%cl, %%eax");
                break;

            case AST_SHR:
                if (is_unsigned) {
                    emit(gen, "    shrl %%cl, %%eax");
                } else {
                    emit(gen, "    sarl %%cl, %%eax");
                }
                break;

            case AST_BITAND:
                emit(gen, "    andl %%ecx, %%eax");
                break;

            case AST_BITOR:
                emit(gen, "    orl %%ecx, %%eax");
                break;

            case AST_BITXOR:
                emit(gen, "    xorl %%ecx, %%eax");
                break;

            case AST_EQ:
                emit(gen, "    cmpl %%ecx, %%eax");
                emit(gen, "    sete %%al");
                emit(gen, "    movzbl %%al, %%eax");
                break;

            case AST_NE:
                emit(gen, "    cmpl %%ecx, %%eax");
                emit(gen, "    setne %%al");
                emit(gen, "    movzbl %%al, %%eax");
                break;

            case AST_LT:
                emit(gen, "    cmpl %%ecx, %%eax");
                if (is_unsigned) {
                    emit(gen, "    setb %%al");
                } else {
                    emit(gen, "    setl %%al");
                }
                emit(gen, "    movzbl %%al, %%eax");
                break;

            case AST_LE:
                emit(gen, "    cmpl %%ecx, %%eax");
                if (is_unsigned) {
                    emit(gen, "    setbe %%al");
                } else {
                    emit(gen, "    setle %%al");
                }
                emit(gen, "    movzbl %%al, %%eax");
                break;

            case AST_GT:
                emit(gen, "    cmpl %%ecx, %%eax");
                if (is_unsigned) {
                    emit(gen, "    seta %%al");
                } else {
                    emit(gen, "    setg %%al");
                }
                emit(gen, "    movzbl %%al, %%eax");
                break;

            case AST_GE:
                emit(gen, "    cmpl %%ecx, %%eax");
                if (is_unsigned) {
                    emit(gen, "    setae %%al");
                } else {
                    emit(gen, "    setge %%al");
                }
                emit(gen, "    movzbl %%al, %%eax");
                break;

            default:
                break;
        }
    }
}

/* ========================================================================= */
/* Statement Code Generation (32-bit i386)                                   */
/* ========================================================================= */

static void gen_local_initializer(CodeGen *gen, Symbol *sym, Type *type, Initializer *init, int base_offset) {
    if (!init) return;

    if (init->is_compound) {
        int i;
        Type *elem_type = (type && type->kind == TYPE_ARRAY) ? type->base : type;
        int elem_size = elem_type ? elem_type->size : 4;
        for (i = 0; i < init->elements->size; i++) {
            Initializer *elem = (Initializer *)vec_get(init->elements, i);
            gen_local_initializer(gen, sym, elem_type, elem, base_offset + i * elem_size);
        }
    } else if (init->expr) {
        Type *target_type;
        if (type && type->kind == TYPE_ARRAY && init->expr->kind == AST_STR_LIT) {
            /* Copy string literal characters into array on stack and zero rest */
            const char *str = init->expr->u.str_val.str;
            int len = init->expr->u.str_val.len;
            int total_size = type->size > 0 ? type->size : (len + 1);
            int i;
            for (i = 0; i < total_size; i++) {
                char ch = (i < len) ? str[i] : 0;
                emit(gen, "    movb $%d, %d(%%ebp)", (unsigned char)ch, sym->stack_offset + base_offset + i);
            }
            return;
        }

        target_type = (type && type->kind == TYPE_ARRAY) ? type->base : type;
        gen_expr(gen, init->expr);
        emit(gen, "    leal %d(%%ebp), %%ecx", sym->stack_offset + base_offset);
        gen_store(gen, target_type);
    }
}

static void gen_stmt(CodeGen *gen, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
        case AST_BLOCK: {
            int i;
            Vector *stmts = node->u.block.stmts;
            if (stmts) {
                for (i = 0; i < stmts->size; i++) {
                    gen_stmt(gen, (AstNode *)vec_get(stmts, i));
                }
            }
            break;
        }

        case AST_DECL_STMT: {
            Symbol *sym = node->u.decl_stmt.sym;
            if (sym->storage != STORAGE_STATIC && node->u.decl_stmt.init) {
                gen_local_initializer(gen, sym, sym->type, node->u.decl_stmt.init, 0);
            }
            break;
        }

        case AST_EXPR_STMT:
            gen_expr(gen, node->u.expr_stmt.expr);
            break;

        case AST_IF: {
            char *label_else = gen_asm_label(gen, "if_else");
            char *label_end = gen_asm_label(gen, "if_end");
            gen_expr(gen, node->u.if_stmt.cond);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    je %s", node->u.if_stmt.else_stmt ? label_else : label_end);
            gen_stmt(gen, node->u.if_stmt.then_stmt);
            if (node->u.if_stmt.else_stmt) {
                emit(gen, "    jmp %s", label_end);
                emit(gen, "%s:", label_else);
                gen_stmt(gen, node->u.if_stmt.else_stmt);
            }
            emit(gen, "%s:", label_end);
            break;
        }

        case AST_WHILE: {
            char *label_start = gen_asm_label(gen, "while_start");
            vec_push(gen->break_stack, node->u.loop_stmt.break_label);
            vec_push(gen->continue_stack, node->u.loop_stmt.continue_label);

            emit(gen, "%s:", label_start);
            emit(gen, "%s:", node->u.loop_stmt.continue_label);
            gen_expr(gen, node->u.loop_stmt.cond);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    je %s", node->u.loop_stmt.break_label);
            gen_stmt(gen, node->u.loop_stmt.body);
            emit(gen, "    jmp %s", label_start);
            emit(gen, "%s:", node->u.loop_stmt.break_label);

            vec_pop(gen->break_stack);
            vec_pop(gen->continue_stack);
            break;
        }

        case AST_DO_WHILE: {
            char *label_start = gen_asm_label(gen, "do_start");
            vec_push(gen->break_stack, node->u.loop_stmt.break_label);
            vec_push(gen->continue_stack, node->u.loop_stmt.continue_label);

            emit(gen, "%s:", label_start);
            gen_stmt(gen, node->u.loop_stmt.body);
            emit(gen, "%s:", node->u.loop_stmt.continue_label);
            gen_expr(gen, node->u.loop_stmt.cond);
            emit(gen, "    cmpl $0, %%eax");
            emit(gen, "    jne %s", label_start);
            emit(gen, "%s:", node->u.loop_stmt.break_label);

            vec_pop(gen->break_stack);
            vec_pop(gen->continue_stack);
            break;
        }

        case AST_FOR: {
            char *label_start = gen_asm_label(gen, "for_start");
            vec_push(gen->break_stack, node->u.for_stmt.break_label);
            vec_push(gen->continue_stack, node->u.for_stmt.continue_label);

            if (node->u.for_stmt.init) {
                gen_expr(gen, node->u.for_stmt.init);
            }
            emit(gen, "%s:", label_start);
            if (node->u.for_stmt.cond) {
                gen_expr(gen, node->u.for_stmt.cond);
                emit(gen, "    cmpl $0, %%eax");
                emit(gen, "    je %s", node->u.for_stmt.break_label);
            }
            gen_stmt(gen, node->u.for_stmt.body);
            emit(gen, "%s:", node->u.for_stmt.continue_label);
            if (node->u.for_stmt.step) {
                gen_expr(gen, node->u.for_stmt.step);
            }
            emit(gen, "    jmp %s", label_start);
            emit(gen, "%s:", node->u.for_stmt.break_label);

            vec_pop(gen->break_stack);
            vec_pop(gen->continue_stack);
            break;
        }

        case AST_SWITCH: {
            int i;
            Vector *cases = node->u.switch_stmt.cases;
            vec_push(gen->break_stack, node->u.switch_stmt.break_label);

            gen_expr(gen, node->u.switch_stmt.cond);
            /* Comparisons for cases */
            for (i = 0; i < cases->size; i++) {
                AstNode *cnode = (AstNode *)vec_get(cases, i);
                if (cnode->kind == AST_CASE) {
                    emit(gen, "    cmpl $%ld, %%eax", cnode->u.case_stmt.val);
                    emit(gen, "    je %s", cnode->u.case_stmt.label);
                }
            }
            if (node->u.switch_stmt.default_label) {
                emit(gen, "    jmp %s", node->u.switch_stmt.default_label);
            } else {
                emit(gen, "    jmp %s", node->u.switch_stmt.break_label);
            }
            gen_stmt(gen, node->u.switch_stmt.body);
            emit(gen, "%s:", node->u.switch_stmt.break_label);

            vec_pop(gen->break_stack);
            break;
        }

        case AST_CASE:
            emit(gen, "%s:", node->u.case_stmt.label);
            if (node->u.case_stmt.stmt) {
                gen_stmt(gen, node->u.case_stmt.stmt);
            }
            break;

        case AST_DEFAULT:
            emit(gen, "%s:", node->u.default_stmt.label);
            if (node->u.default_stmt.stmt) {
                gen_stmt(gen, node->u.default_stmt.stmt);
            }
            break;

        case AST_BREAK:
            if (gen->break_stack->size > 0) {
                char *lbl = (char *)vec_get(gen->break_stack, gen->break_stack->size - 1);
                emit(gen, "    jmp %s", lbl);
            }
            break;

        case AST_CONTINUE:
            if (gen->continue_stack->size > 0) {
                char *lbl = (char *)vec_get(gen->continue_stack, gen->continue_stack->size - 1);
                emit(gen, "    jmp %s", lbl);
            }
            break;

        case AST_RETURN:
            if (node->u.return_stmt.expr) {
                gen_expr(gen, node->u.return_stmt.expr);
            }
            if (gen->func_ret_label) {
                emit(gen, "    jmp %s", gen->func_ret_label);
            }
            break;

        case AST_GOTO:
            emit(gen, "    jmp .L_user_%s", node->u.label_stmt.name);
            break;

        case AST_LABEL:
            emit(gen, ".L_user_%s:", node->u.label_stmt.name);
            if (node->u.label_stmt.stmt) {
                gen_stmt(gen, node->u.label_stmt.stmt);
            }
            break;

        default:
            break;
    }
}

/* ========================================================================= */
/* Global Variables & Data Sections (32-bit i386)                            */
/* ========================================================================= */

static void gen_global_init(CodeGen *gen, Initializer *init, Type *type) {
    if (!init) {
        emit(gen, "    .zero %d", type ? (type->size > 0 ? type->size : 4) : 4);
        return;
    }

    if (init->is_compound) {
        int i;
        if (type && type->kind == TYPE_STRUCT) {
            Member *m = type->members;
            int cur_offset = 0;
            for (i = 0; i < init->elements->size && m; i++, m = m->next) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                if (m->offset > cur_offset) {
                    emit(gen, "    .zero %d", m->offset - cur_offset);
                    cur_offset = m->offset;
                }
                gen_global_init(gen, elem, m->type);
                cur_offset += (m->type && m->type->size > 0) ? m->type->size : 4;
            }
            if (cur_offset < type->size) {
                emit(gen, "    .zero %d", type->size - cur_offset);
            }
        } else if (type && type->kind == TYPE_ARRAY) {
            Type *elem_type = type->base ? type->base : type_int;
            for (i = 0; i < init->elements->size; i++) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                gen_global_init(gen, elem, elem_type);
            }
        } else {
            for (i = 0; i < init->elements->size; i++) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                gen_global_init(gen, elem, type->base ? type->base : type_int);
            }
        }
    } else if (init->expr) {
        AstNode *e = init->expr;
        if (e->kind == AST_INT_LIT || e->kind == AST_CHAR_LIT) {
            if (type->size == 1) emit(gen, "    .byte %ld", e->u.int_val.val);
            else if (type->size == 2) emit(gen, "    .value %ld", e->u.int_val.val);
            else if (type->size == 4) emit(gen, "    .long %ld", e->u.int_val.val);
            else emit(gen, "    .quad %ld", e->u.int_val.val);
        } else if (e->kind == AST_FLOAT_LIT) {
            if (type && type->kind == TYPE_FLOAT) {
                emit(gen, "    .long %u", e->u.float_val.u128_words[0]);
            } else if (type && type->kind == TYPE_LDOUBLE) {
                emit(gen, "    .long %u", e->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", e->u.float_val.u128_words[1]);
                emit(gen, "    .long %u", e->u.float_val.u128_words[2]);
                emit(gen, "    .long %u", e->u.float_val.u128_words[3]);
            } else {
                emit(gen, "    .long %u", e->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", e->u.float_val.u128_words[1]);
            }
        } else if (e->kind == AST_STR_LIT) {
            emit(gen, "    .long %s", e->u.str_val.label);
        } else if (e->kind == AST_ADDR && e->u.unop.operand->kind == AST_VAR) {
            emit(gen, "    .long %s", e->u.unop.operand->u.sym->name);
        } else {
            emit(gen, "    .zero %d", type->size);
        }
    }
}

static void gen_globals(CodeGen *gen, Vector *globals) {
    int i;
    if (!globals) return;

    for (i = 0; i < globals->size; i++) {
        Symbol *sym = (Symbol *)vec_get(globals, i);
        const char *gname = sym->asm_label ? sym->asm_label : sym->name;

        if (sym->storage == STORAGE_EXTERN) continue;

        if (sym->storage != STORAGE_STATIC) {
            emit(gen, "    .globl %s", gname);
        }

        if (sym->init) {
            emit(gen, "    .data");
            emit(gen, "    .align %d", sym->type->align);
            emit(gen, "%s:", gname);
            gen_global_init(gen, sym->init, sym->type);
        } else {
            emit(gen, "    .bss");
            emit(gen, "    .align %d", sym->type->align);
            emit(gen, "%s:", gname);
            emit(gen, "    .zero %d", sym->type->size > 0 ? sym->type->size : 4);
        }
    }
}

static void gen_rodata(CodeGen *gen, Vector *strings, Vector *floats) {
    int i;
    if ((!strings || strings->size == 0) && (!floats || floats->size == 0)) return;

    emit(gen, "    .section .rodata");
    if (strings) {
        for (i = 0; i < strings->size; i++) {
            AstNode *s = (AstNode *)vec_get(strings, i);
            int j;
            emit(gen, "%s:", s->u.str_val.label);
            for (j = 0; j < s->u.str_val.len; j++) {
                emit(gen, "    .byte %d", (unsigned char)s->u.str_val.str[j]);
            }
            emit(gen, "    .byte 0");
        }
    }
    if (floats) {
        for (i = 0; i < floats->size; i++) {
            AstNode *f = (AstNode *)vec_get(floats, i);
            emit(gen, "%s:", f->u.float_val.label);
            if (f->type == type_float) {
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
            } else if (f->type == type_ldouble) {
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[1]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[2]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[3]);
            } else {
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[1]);
            }
        }
    }
}

/* ========================================================================= */
/* Function Definition Code Generation (32-bit i386)                         */
/* ========================================================================= */

static void gen_func_def(CodeGen *gen, AstNode *func_node) {
    Symbol *sym = func_node->u.func_def.sym;
    int stack_size = func_node->u.func_def.stack_size;

    gen->func_ret_label = gen_asm_label(gen, "ret");
    gen->scratch_base = (stack_size > 128) ? (stack_size - 128 + 16) : 16;
    gen->ldouble_slot = 0;

    emit(gen, "    .text");
    if (sym->storage != STORAGE_STATIC) {
        emit(gen, "    .globl %s", sym->name);
    }
    emit(gen, "    .type %s, @function", sym->name);
    emit(gen, "%s:", sym->name);
    emit(gen, "    pushl %%ebp");
    emit(gen, "    movl %%esp, %%ebp");
    if (stack_size > 0) {
        emit(gen, "    subl $%d, %%esp", stack_size);
    }

    /* In cdecl ABI, all parameters are already on the stack at 8(%ebp)+ */

    /* Generate function body statements */
    gen_stmt(gen, func_node->u.func_def.body);

    /* Function Epilogue */
    emit(gen, "%s:", gen->func_ret_label);
    emit(gen, "    movl %%ebp, %%esp");
    emit(gen, "    popl %%ebp");
    emit(gen, "    ret");

    free(gen->func_ret_label);
    gen->func_ret_label = NULL;
}

void codegen_generate(CodeGen *gen) {
    AstNode *unit = gen->root;
    int i;
    Vector *decls;

    if (!unit || unit->kind != AST_TRANSLATION_UNIT) return;

    /* Emit read-only data (strings and floats) */
    gen_rodata(gen, unit->u.trans_unit.strings, unit->u.trans_unit.floats);

    /* Emit global variables */
    gen_globals(gen, unit->u.trans_unit.globals);

    /* Emit function definitions */
    decls = unit->u.trans_unit.decls;
    for (i = 0; i < decls->size; i++) {
        AstNode *decl = (AstNode *)vec_get(decls, i);
        if (decl->kind == AST_FUNC_DEF) {
            gen_func_def(gen, decl);
        }
    }
}
