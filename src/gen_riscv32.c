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
    gen->current_func = NULL;
    return gen;
}

static char *gen_asm_label(CodeGen *gen, const char *prefix) {
    char buf[64];
    sprintf(buf, ".L_%s_%d", prefix, gen->label_count++);
    return c90_strdup(buf);
}

static void emit(CodeGen *gen, const char *fmt, ...) {
    const char *args[6];
    const char *p = fmt;
    int arg_idx = 0;
    int is_long = 0;
    int i;
    va_list ap;
    va_start(ap, fmt);
    for (i = 0; i < 6; i++) {
        args[i] = va_arg(ap, char *);
    }
    va_end(ap);

    while (*p) {
        if (*p == '%' && *(p + 1) == '%') {
            fputc('%', gen->out);
            p += 2;
        } else if (*p == '%') {
            p++;
            is_long = 0;
            if (*p == 'l') { is_long = 1; p++; }
            if (*p == 's') {
                const char *s = (arg_idx < 6) ? args[arg_idx] : NULL;
                if (s) fputs(s, gen->out);
                arg_idx++;
                p++;
            } else if (*p == 'd' || *p == 'i') {
                if (is_long) {
                    long val = (arg_idx < 6) ? (long)args[arg_idx] : 0;
                    fprintf(gen->out, "%ld", val);
                } else {
                    int val = (arg_idx < 6) ? (int)(long)args[arg_idx] : 0;
                    fprintf(gen->out, "%d", val);
                }
                arg_idx++;
                p++;
            } else if (*p == 'u') {
                if (is_long) {
                    unsigned long val = (arg_idx < 6) ? (unsigned long)args[arg_idx] : 0;
                    fprintf(gen->out, "%lu", val);
                } else {
                    unsigned int val = (arg_idx < 6) ? (unsigned int)(long)args[arg_idx] : 0;
                    fprintf(gen->out, "%u", val);
                }
                arg_idx++;
                p++;
            } else if (*p == 'c') {
                int val = (arg_idx < 6) ? (int)(long)args[arg_idx] : 0;
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

static void emit_addi(CodeGen *gen, const char *rd, const char *rs, int imm) {
    if (imm >= -2048 && imm <= 2047) {
        emit(gen, "    addi %s, %s, %d", rd, rs, imm);
    } else {
        const char *tmp = (strcmp(rs, "t0") == 0 || strcmp(rd, "t0") == 0) ? "t1" : "t0";
        emit(gen, "    li %s, %d", tmp, imm);
        emit(gen, "    add %s, %s, %s", rd, rs, tmp);
    }
}

static void emit_sw_s0(CodeGen *gen, const char *src_reg, int off) {
    if (off >= -2048 && off <= 2047) {
        emit(gen, "    sw %s, %d(s0)", src_reg, off);
    } else {
        const char *tmp = (strcmp(src_reg, "t0") == 0) ? "t1" : "t0";
        emit_addi(gen, tmp, "s0", off);
        emit(gen, "    sw %s, 0(%s)", src_reg, tmp);
    }
}

static void emit_lw_s0(CodeGen *gen, const char *dst_reg, int off) {
    if (off >= -2048 && off <= 2047) {
        emit(gen, "    lw %s, %d(s0)", dst_reg, off);
    } else {
        const char *tmp = (strcmp(dst_reg, "t0") == 0) ? "t1" : "t0";
        emit_addi(gen, tmp, "s0", off);
        emit(gen, "    lw %s, 0(%s)", dst_reg, tmp);
    }
}

/* ========================================================================= */
/* Memory Load and Store Helper Functions (32-bit RISC-V RV32I)              */
/* ========================================================================= */

static void gen_load(CodeGen *gen, Type *type) {
    if (!type) {
        emit(gen, "    lw a0, 0(a0)");
        return;
    }
    if (type->kind == TYPE_ARRAY || type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
        type->kind == TYPE_FUNC || type->kind == TYPE_LDOUBLE || type->kind == TYPE_DOUBLE) {
        /* Array/struct/func/ldouble/double evaluates to its base address */
        return;
    }
    if (type->size == 1) {
        if (type->is_unsigned) {
            emit(gen, "    lbu a0, 0(a0)");
        } else {
            emit(gen, "    lb a0, 0(a0)");
        }
    } else if (type->size == 2) {
        if (type->is_unsigned) {
            emit(gen, "    lhu a0, 0(a0)");
        } else {
            emit(gen, "    lh a0, 0(a0)");
        }
    } else {
        emit(gen, "    lw a0, 0(a0)");
    }
}

static void gen_store(CodeGen *gen, Type *type) {
    if (!type) {
        emit(gen, "    sw a0, 0(a1)");
        return;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION || type->kind == TYPE_LDOUBLE || type->size > 4) {
        /* Copy multi-word struct/union/ldouble/double */
        if (type->size <= 16) {
            int i;
            for (i = 0; i < type->size; i++) {
                emit(gen, "    lbu t0, %d(a0)", i);
                emit(gen, "    sb t0, %d(a1)", i);
            }
        } else {
            char *lbl = gen_asm_label(gen, "cpy");
            emit(gen, "    li t1, %d", type->size);
            emit(gen, "%s:", lbl);
            emit(gen, "    lbu t0, 0(a0)");
            emit(gen, "    sb t0, 0(a1)");
            emit(gen, "    addi a0, a0, 1");
            emit(gen, "    addi a1, a1, 1");
            emit(gen, "    addi t1, t1, -1");
            emit(gen, "    bnez t1, %s", lbl);
        }
        return;
    }
    if (type->size == 1) {
        emit(gen, "    sb a0, 0(a1)");
    } else if (type->size == 2) {
        emit(gen, "    sh a0, 0(a1)");
    } else {
        emit(gen, "    sw a0, 0(a1)");
    }
}

static void gen_bitfield_load(CodeGen *gen, Member *m) {
    if (!m || m->bit_width <= 0) return;
    if (m->type && m->type->is_unsigned) {
        if (m->bit_offset > 0) {
            emit(gen, "    srli a0, a0, %d", m->bit_offset);
        }
        if (m->bit_width < 32) {
            unsigned int mask = (1U << m->bit_width) - 1U;
            emit(gen, "    li t0, %u", mask);
            emit(gen, "    and a0, a0, t0");
        }
    } else {
        int sh = 32 - (m->bit_offset + m->bit_width);
        if (sh > 0) {
            emit(gen, "    slli a0, a0, %d", sh);
        }
        emit(gen, "    srai a0, a0, %d", 32 - m->bit_width);
    }
}

static void gen_bitfield_store(CodeGen *gen, Member *m) {
    unsigned int mask = (1U << m->bit_width) - 1U;
    unsigned int clear_mask = ~(mask << m->bit_offset);
    emit(gen, "    li t0, %u", mask);
    emit(gen, "    and a0, a0, t0");
    if (m->bit_offset > 0) {
        emit(gen, "    slli a0, a0, %d", m->bit_offset);
    }
    emit(gen, "    mv t1, a0"); /* New bits in t1 */
    if (m->type->size == 1) emit(gen, "    lbu a0, 0(a1)");
    else if (m->type->size == 2) emit(gen, "    lhu a0, 0(a1)");
    else emit(gen, "    lw a0, 0(a1)");
    if (m->type->size == 1) {
        emit(gen, "    li t0, %u", (unsigned int)(clear_mask & 0xFF));
        emit(gen, "    and a0, a0, t0");
    } else if (m->type->size == 2) {
        emit(gen, "    li t0, %u", (unsigned int)(clear_mask & 0xFFFF));
        emit(gen, "    and a0, a0, t0");
    } else {
        emit(gen, "    li t0, %u", clear_mask);
        emit(gen, "    and a0, a0, t0");
    }
    emit(gen, "    or a0, a0, t1");
    if (m->type->size == 1) emit(gen, "    sb a0, 0(a1)");
    else if (m->type->size == 2) emit(gen, "    sh a0, 0(a1)");
    else emit(gen, "    sw a0, 0(a1)");
    emit(gen, "    mv a0, t1");
    if (m->type && m->type->is_unsigned) {
        if (m->bit_offset > 0) emit(gen, "    srli a0, a0, %d", m->bit_offset);
    } else {
        int sh = 32 - (m->bit_offset + m->bit_width);
        if (sh > 0) emit(gen, "    slli a0, a0, %d", sh);
        emit(gen, "    srai a0, a0, %d", 32 - m->bit_width);
    }
}

/* ========================================================================= */
/* Lvalue Address Generation (32-bit RISC-V RV32I)                           */
/* ========================================================================= */

static void gen_lval(CodeGen *gen, AstNode *node) {
    if (!node) return;

    if (node->kind == AST_VAR) {
        Symbol *sym = node->u.sym;
        if (sym->is_global || sym->kind == SYM_FUNC || sym->asm_label) {
            if (sym->asm_label) {
                emit(gen, "    la a0, %s", sym->asm_label);
            } else {
                emit(gen, "    la a0, %s", sym->name);
            }
        } else {
            emit_addi(gen, "a0", "s0", sym->stack_offset);
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
            emit_addi(gen, "a0", "a0", node->u.member.member->offset);
        }
        return;
    }

    c90_error(node->filename, node->line, "expression is not an lvalue");
}

/* ========================================================================= */
/* Expression Code Generation (32-bit RISC-V RV32I)                          */
/* ========================================================================= */

static int get_scratch_temp(CodeGen *gen) {
    int stack_size = gen->current_stack_size ? gen->current_stack_size : 160;
    int off = -stack_size + 8 + (gen->ldouble_slot * 16);
    gen->ldouble_slot = (gen->ldouble_slot + 1) % 8;
    return off;
}

static void gen_cast_to(CodeGen *gen, Type *from, Type *to) {
    if (!from || !to || type_equal(from, to) || to->kind == TYPE_VOID) return;

    if (to->kind == TYPE_FLOAT) {
        if (from->kind == TYPE_DOUBLE) {
            emit(gen, "    call __truncdfsf2");
        } else if (from->kind == TYPE_LDOUBLE) {
            emit(gen, "    call __trunctfsf2");
        } else if (from->is_unsigned) {
            emit(gen, "    call __floatunsisf");
        } else {
            emit(gen, "    call __floatsisf");
        }
        return;
    }

    if (to->kind == TYPE_DOUBLE) {
        int off = get_scratch_temp(gen);
        if (from->kind == TYPE_FLOAT) {
            emit(gen, "    call __extendsfdf2");
        } else if (from->kind == TYPE_LDOUBLE) {
            emit(gen, "    call __trunctfdf2");
        } else if (from->is_unsigned) {
            emit(gen, "    call __floatunsidf");
        } else {
            emit(gen, "    call __floatsidf");
        }
        emit_addi(gen, "t0", "s0", off);
        emit(gen, "    sw a0, 0(t0)");
        emit(gen, "    sw a1, 4(t0)");
        emit_addi(gen, "a0", "s0", off);
        return;
    }

    if (to->kind == TYPE_LDOUBLE) {
        int off = get_scratch_temp(gen);
        emit(gen, "    mv a1, a0");
        emit_addi(gen, "a0", "s0", off);
        if (from->kind == TYPE_FLOAT) {
            emit(gen, "    call __extendsftf2");
        } else if (from->kind == TYPE_DOUBLE) {
            emit(gen, "    call __extenddftf2");
        } else if (from->is_unsigned) {
            emit(gen, "    call __floatunsitf");
        } else {
            emit(gen, "    call __floatsitf");
        }
        emit_addi(gen, "a0", "s0", off);
        return;
    }

    if (from->kind == TYPE_FLOAT) {
        if (to->is_unsigned) emit(gen, "    call __fixunssfsi");
        else emit(gen, "    call __fixsfsi");
        return;
    }

    if (from->kind == TYPE_DOUBLE) {
        emit(gen, "    lw a1, 4(a0)");
        emit(gen, "    lw a0, 0(a0)");
        if (to->kind == TYPE_FLOAT) {
            emit(gen, "    call __truncdfsf2");
        } else if (to->is_unsigned) {
            emit(gen, "    call __fixunsdfsi");
        } else {
            emit(gen, "    call __fixdfsi");
        }
        return;
    }

    if (from->kind == TYPE_LDOUBLE) {
        if (to->is_unsigned) emit(gen, "    call __fixunstfsi");
        else emit(gen, "    call __fixtfsi");
        return;
    }

    if (to->size == 1) {
        if (to->is_unsigned) {
            emit(gen, "    andi a0, a0, 255");
        } else {
            emit(gen, "    slli a0, a0, 24");
            emit(gen, "    srai a0, a0, 24");
        }
    } else if (to->size == 2) {
        if (to->is_unsigned) {
            emit(gen, "    slli a0, a0, 16");
            emit(gen, "    srli a0, a0, 16");
        } else {
            emit(gen, "    slli a0, a0, 16");
            emit(gen, "    srai a0, a0, 16");
        }
    }
}

static int is_multiword_arg(Type *type) {
    if (!type) return 0;
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION ||
        type->kind == TYPE_DOUBLE || type->kind == TYPE_LDOUBLE ||
        ((type->kind == TYPE_INT || type->kind == TYPE_LONG) && type->size > 4)) {
        return 1;
    }
    return 0;
}

static int needs_8byte_align(Type *type) {
    if (!type) return 0;
    if (type->kind == TYPE_DOUBLE || type->kind == TYPE_LDOUBLE ||
        ((type->kind == TYPE_INT || type->kind == TYPE_LONG) && type->size >= 8)) {
        return 1;
    }
    if ((type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) && type->align >= 8) {
        return 1;
    }
    return 0;
}

static void gen_expr(CodeGen *gen, AstNode *node) {
    if (!node) return;

    switch (node->kind) {
    case AST_INT_LIT:
    case AST_CHAR_LIT:
        emit(gen, "    li a0, %d", (int)node->u.int_val.val);
        return;

    case AST_FLOAT_LIT:
        if (node->u.float_val.label) {
            if (node->type == type_float) {
                emit(gen, "    la a0, %s", node->u.float_val.label);
                emit(gen, "    lw a0, 0(a0)");
            } else {
                emit(gen, "    la a0, %s", node->u.float_val.label);
            }
        } else {
            emit(gen, "    li a0, 0");
        }
        return;

    case AST_STR_LIT:
        emit(gen, "    la a0, %s", node->u.str_val.label);
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
            emit(gen, "    call __negsf2");
        } else if (node->type && node->type->kind == TYPE_DOUBLE) {
            int off = get_scratch_temp(gen);
            emit(gen, "    lw a1, 4(a0)");
            emit(gen, "    lw a0, 0(a0)");
            emit(gen, "    call __negdf2");
            emit_addi(gen, "t0", "s0", off);
            emit(gen, "    sw a0, 0(t0)");
            emit(gen, "    sw a1, 4(t0)");
            emit_addi(gen, "a0", "s0", off);
        } else if (node->type && node->type->kind == TYPE_LDOUBLE) {
            int off = get_scratch_temp(gen);
            emit(gen, "    mv a1, a0");
            emit_addi(gen, "a0", "s0", off);
            emit(gen, "    call __negtf2");
            emit_addi(gen, "a0", "s0", off);
        } else {
            emit(gen, "    neg a0, a0");
        }
        return;

    case AST_BITNOT:
        gen_expr(gen, node->u.unop.operand);
        emit(gen, "    not a0, a0");
        return;

    case AST_LOGNOT:
        gen_expr(gen, node->u.unop.operand);
        emit(gen, "    seqz a0, a0");
        return;

    case AST_PRE_INC:
    case AST_PRE_DEC: {
        int is_inc = (node->kind == AST_PRE_INC);
        int step = 1;
        Member *m = (node->u.unop.operand->kind == AST_MEMBER) ? node->u.unop.operand->u.member.member : NULL;
        if (node->type && node->type->kind == TYPE_PTR && node->type->base) {
            step = node->type->base->size > 0 ? node->type->base->size : 1;
        }
        gen_lval(gen, node->u.unop.operand);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)"); /* push lval address */

        if (node->type && type_is_floating(node->type)) {
            if (node->type->kind == TYPE_FLOAT) {
                gen_load(gen, node->type);
                emit(gen, "    lui a1, 0x3f800"); /* 1.0f */
                if (is_inc) emit(gen, "    call __addsf3");
                else emit(gen, "    call __subsf3");
                emit(gen, "    lw a1, 0(sp)");
                emit(gen, "    sw a0, 0(a1)");
                emit(gen, "    addi sp, sp, 16");
            } else if (node->type->kind == TYPE_DOUBLE) {
                emit(gen, "    lw a0, 0(sp)");
                emit(gen, "    lw a1, 4(a0)");
                emit(gen, "    lw a0, 0(a0)");
                emit(gen, "    li a2, 0");
                emit(gen, "    lui a3, 0x3ff00");
                if (is_inc) emit(gen, "    call __adddf3");
                else emit(gen, "    call __subdf3");
                emit(gen, "    lw a2, 0(sp)");
                emit(gen, "    sw a0, 0(a2)");
                emit(gen, "    sw a1, 4(a2)");
                emit(gen, "    mv a0, a2");
                emit(gen, "    addi sp, sp, 16");
            } else if (node->type->kind == TYPE_LDOUBLE) {
                int off_one = get_scratch_temp(gen);
                int off_res = get_scratch_temp(gen);
                emit_addi(gen, "a0", "s0", off_one);
                emit(gen, "    li a1, 1");
                emit(gen, "    call __floatsitf");
                emit(gen, "    lw a1, 0(sp)");
                emit_addi(gen, "a2", "s0", off_one);
                emit_addi(gen, "a0", "s0", off_res);
                if (is_inc) emit(gen, "    call __addtf3");
                else emit(gen, "    call __subtf3");
                emit(gen, "    lw a1, 0(sp)");
                emit_addi(gen, "a0", "s0", off_res);
                gen_store(gen, node->type);
                emit(gen, "    addi sp, sp, 16");
                emit_addi(gen, "a0", "s0", off_res);
            }
            return;
        }

        gen_load(gen, node->type);
        if (m && m->bit_width > 0) gen_bitfield_load(gen, m);
        if (is_inc) {
            emit(gen, "    addi a0, a0, %d", step);
        } else {
            emit(gen, "    addi a0, a0, -%d", step);
        }
        if (m && m->bit_width > 0) {
            emit(gen, "    lw a1, 0(sp)");
            gen_bitfield_store(gen, m);
            emit(gen, "    addi sp, sp, 16");
        } else {
            emit(gen, "    lw a1, 0(sp)");
            gen_store(gen, node->type);
            emit(gen, "    addi sp, sp, 16");
        }
        return;
    }

    case AST_POST_INC:
    case AST_POST_DEC: {
        int is_inc = (node->kind == AST_POST_INC);
        int step = 1;
        Member *m = (node->u.unop.operand->kind == AST_MEMBER) ? node->u.unop.operand->u.member.member : NULL;
        if (node->type && node->type->kind == TYPE_PTR && node->type->base) {
            step = node->type->base->size > 0 ? node->type->base->size : 1;
        }
        gen_lval(gen, node->u.unop.operand);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)"); /* push lval address */

        if (node->type && type_is_floating(node->type)) {
            if (node->type->kind == TYPE_FLOAT) {
                gen_load(gen, node->type);
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    sw a0, 0(sp)"); /* push original value */
                emit(gen, "    lui a1, 0x3f800"); /* 1.0f */
                if (is_inc) emit(gen, "    call __addsf3");
                else emit(gen, "    call __subsf3");
                emit(gen, "    lw a1, 16(sp)");
                emit(gen, "    sw a0, 0(a1)");
                emit(gen, "    lw a0, 0(sp)"); /* restore original */
                emit(gen, "    addi sp, sp, 32");
            } else if (node->type->kind == TYPE_DOUBLE) {
                int off_orig = get_scratch_temp(gen);
                emit(gen, "    lw a0, 0(sp)");
                emit(gen, "    lw a1, 4(a0)");
                emit(gen, "    lw a0, 0(a0)");
                emit_addi(gen, "t0", "s0", off_orig);
                emit(gen, "    sw a0, 0(t0)");
                emit(gen, "    sw a1, 4(t0)");
                emit(gen, "    li a2, 0");
                emit(gen, "    lui a3, 0x3ff00");
                if (is_inc) emit(gen, "    call __adddf3");
                else emit(gen, "    call __subdf3");
                emit(gen, "    lw a2, 0(sp)");
                emit(gen, "    sw a0, 0(a2)");
                emit(gen, "    sw a1, 4(a2)");
                emit(gen, "    addi sp, sp, 16");
                emit_addi(gen, "a0", "s0", off_orig);
            } else if (node->type->kind == TYPE_LDOUBLE) {
                int off_orig = get_scratch_temp(gen);
                int off_one = get_scratch_temp(gen);
                int off_res = get_scratch_temp(gen);
                /* Save original value */
                emit(gen, "    lw a1, 0(sp)");
                emit_addi(gen, "a0", "s0", off_orig);
                gen_store(gen, node->type);
                /* Prepare 1.0 */
                emit_addi(gen, "a0", "s0", off_one);
                emit(gen, "    li a1, 1");
                emit(gen, "    call __floatsitf");
                emit(gen, "    lw a1, 0(sp)");
                emit_addi(gen, "a2", "s0", off_one);
                emit_addi(gen, "a0", "s0", off_res);
                if (is_inc) emit(gen, "    call __addtf3");
                else emit(gen, "    call __subtf3");
                emit(gen, "    lw a1, 0(sp)");
                emit_addi(gen, "a0", "s0", off_res);
                gen_store(gen, node->type);
                emit(gen, "    addi sp, sp, 16");
                emit_addi(gen, "a0", "s0", off_orig);
            }
            return;
        }

        gen_load(gen, node->type);
        if (m && m->bit_width > 0) gen_bitfield_load(gen, m);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)"); /* push original value */
        if (is_inc) {
            emit(gen, "    addi a0, a0, %d", step);
        } else {
            emit(gen, "    addi a0, a0, -%d", step);
        }
        if (m && m->bit_width > 0) {
            emit(gen, "    lw a1, 16(sp)");
            gen_bitfield_store(gen, m);
            emit(gen, "    lw a0, 0(sp)");
            emit(gen, "    addi sp, sp, 32");
        } else {
            emit(gen, "    lw a1, 16(sp)");
            gen_store(gen, node->type);
            emit(gen, "    lw a0, 0(sp)");
            emit(gen, "    addi sp, sp, 32");
        }
        return;
    }

    case AST_MEMBER: {
        Member *m = node->u.member.member;
        gen_lval(gen, node);
        gen_load(gen, node->type);
        if (m && m->bit_width > 0) gen_bitfield_load(gen, m);
        return;
    }

    case AST_CAST:
        gen_expr(gen, node->u.cast.operand);
        gen_cast_to(gen, node->u.cast.operand->type, node->type);
        return;

    case AST_ASSIGN: {
        AstNode *lhs = node->u.binop.lhs;
        Member *m = (lhs->kind == AST_MEMBER) ? lhs->u.member.member : NULL;
        gen_lval(gen, lhs);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)");
        gen_expr(gen, node->u.binop.rhs);
        if (lhs->type && node->u.binop.rhs->type &&
            !type_equal(lhs->type, node->u.binop.rhs->type)) {
            gen_cast_to(gen, node->u.binop.rhs->type, lhs->type);
        }
        if (m && m->bit_width > 0) {
            emit(gen, "    lw a1, 0(sp)");
            gen_bitfield_store(gen, m);
            emit(gen, "    addi sp, sp, 16");
        } else {
            emit(gen, "    lw a1, 0(sp)");
            gen_store(gen, node->type);
            emit(gen, "    addi sp, sp, 16");
        }
        return;
    }

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
        AstNode *lhs = node->u.binop.lhs;
        Member *m = (lhs->kind == AST_MEMBER) ? lhs->u.member.member : NULL;
        int step = 1;
        int is_unsigned = node->type && node->type->is_unsigned;
        if (node->type && node->type->base) step = node->type->base->size;
        gen_lval(gen, lhs);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)");
        gen_load(gen, node->type);
        if (m && m->bit_width > 0) gen_bitfield_load(gen, m);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)");
        gen_expr(gen, node->u.binop.rhs);
        if (node->type && type_is_floating(node->type)) {
            if (node->u.binop.rhs->type && !type_equal(node->u.binop.rhs->type, node->type)) {
                gen_cast_to(gen, node->u.binop.rhs->type, node->type);
            }
        }
        emit(gen, "    mv a1, a0");
        emit(gen, "    lw a0, 0(sp)");
        emit(gen, "    addi sp, sp, 16");

        if (node->type && type_is_floating(node->type)) {
            if (node->type->kind == TYPE_FLOAT) {
                if (node->kind == AST_ADD_ASSIGN) emit(gen, "    call __addsf3");
                else if (node->kind == AST_SUB_ASSIGN) emit(gen, "    call __subsf3");
                else if (node->kind == AST_MUL_ASSIGN) emit(gen, "    call __mulsf3");
                else if (node->kind == AST_DIV_ASSIGN) emit(gen, "    call __divsf3");
            } else if (node->type->kind == TYPE_DOUBLE) {
                emit(gen, "    lw a2, 0(a1)");
                emit(gen, "    lw a3, 4(a1)");
                emit(gen, "    lw a1, 4(a0)");
                emit(gen, "    lw a0, 0(a0)");
                if (node->kind == AST_ADD_ASSIGN) emit(gen, "    call __adddf3");
                else if (node->kind == AST_SUB_ASSIGN) emit(gen, "    call __subdf3");
                else if (node->kind == AST_MUL_ASSIGN) emit(gen, "    call __muldf3");
                else if (node->kind == AST_DIV_ASSIGN) emit(gen, "    call __divdf3");
                emit(gen, "    lw a2, 0(sp)");
                emit(gen, "    sw a0, 0(a2)");
                emit(gen, "    sw a1, 4(a2)");
                emit(gen, "    mv a0, a2");
                emit(gen, "    addi sp, sp, 16");
                return;
            } else if (node->type->kind == TYPE_LDOUBLE) {
                int off = get_scratch_temp(gen);
                emit(gen, "    mv a2, a1");
                emit(gen, "    mv a1, a0");
                emit_addi(gen, "a0", "s0", off);
                if (node->kind == AST_ADD_ASSIGN) emit(gen, "    call __addtf3");
                else if (node->kind == AST_SUB_ASSIGN) emit(gen, "    call __subtf3");
                else if (node->kind == AST_MUL_ASSIGN) emit(gen, "    call __multf3");
                else if (node->kind == AST_DIV_ASSIGN) emit(gen, "    call __divtf3");
                emit_addi(gen, "a0", "s0", off);
            }
        } else if (node->kind == AST_ADD_ASSIGN) {
            if (step > 1) {
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    sw a0, 0(sp)");
                emit(gen, "    li a0, %d", step);
                emit(gen, "    call __mulsi3");
                emit(gen, "    mv a1, a0");
                emit(gen, "    lw a0, 0(sp)");
                emit(gen, "    addi sp, sp, 16");
            }
            emit(gen, "    add a0, a0, a1");
        } else if (node->kind == AST_SUB_ASSIGN) {
            if (step > 1) {
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    sw a0, 0(sp)");
                emit(gen, "    li a0, %d", step);
                emit(gen, "    call __mulsi3");
                emit(gen, "    mv a1, a0");
                emit(gen, "    lw a0, 0(sp)");
                emit(gen, "    addi sp, sp, 16");
            }
            emit(gen, "    sub a0, a0, a1");
        } else if (node->kind == AST_MUL_ASSIGN) {
            emit(gen, "    call __mulsi3");
        } else if (node->kind == AST_DIV_ASSIGN) {
            if (is_unsigned) emit(gen, "    call __udivsi3");
            else emit(gen, "    call __divsi3");
        } else if (node->kind == AST_MOD_ASSIGN) {
            if (is_unsigned) emit(gen, "    call __umodsi3");
            else emit(gen, "    call __modsi3");
        } else if (node->kind == AST_SHL_ASSIGN) {
            emit(gen, "    sll a0, a0, a1");
        } else if (node->kind == AST_SHR_ASSIGN) {
            if (is_unsigned) emit(gen, "    srl a0, a0, a1");
            else emit(gen, "    sra a0, a0, a1");
        } else if (node->kind == AST_AND_ASSIGN) {
            emit(gen, "    and a0, a0, a1");
        } else if (node->kind == AST_XOR_ASSIGN) {
            emit(gen, "    xor a0, a0, a1");
        } else if (node->kind == AST_OR_ASSIGN) {
            emit(gen, "    or a0, a0, a1");
        }

        if (m && m->bit_width > 0) {
            emit(gen, "    lw a1, 0(sp)");
            gen_bitfield_store(gen, m);
            emit(gen, "    addi sp, sp, 16");
        } else {
            emit(gen, "    lw a1, 0(sp)");
            gen_store(gen, node->type);
            emit(gen, "    addi sp, sp, 16");
        }
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
        emit(gen, "    beqz a0, %s", label_false);
        gen_expr(gen, node->u.binop.rhs);
        emit(gen, "    beqz a0, %s", label_false);
        emit(gen, "    li a0, 1");
        emit(gen, "    j %s", label_end);
        emit(gen, "%s:", label_false);
        emit(gen, "    li a0, 0");
        emit(gen, "%s:", label_end);
        return;
    }

    case AST_LOGOR: {
        char *label_true = gen_asm_label(gen, "lor_true");
        char *label_end = gen_asm_label(gen, "lor_end");
        gen_expr(gen, node->u.binop.lhs);
        emit(gen, "    bnez a0, %s", label_true);
        gen_expr(gen, node->u.binop.rhs);
        emit(gen, "    bnez a0, %s", label_true);
        emit(gen, "    li a0, 0");
        emit(gen, "    j %s", label_end);
        emit(gen, "%s:", label_true);
        emit(gen, "    li a0, 1");
        emit(gen, "%s:", label_end);
        return;
    }

    case AST_COND: {
        char *label_else = gen_asm_label(gen, "cond_else");
        char *label_end = gen_asm_label(gen, "cond_end");
        gen_expr(gen, node->u.cond.cond);
        emit(gen, "    beqz a0, %s", label_else);
        gen_expr(gen, node->u.cond.then_expr);
        emit(gen, "    j %s", label_end);
        emit(gen, "%s:", label_else);
        gen_expr(gen, node->u.cond.else_expr);
        emit(gen, "%s:", label_end);
        return;
    }

    case AST_CALL: {
        Vector *args = node->u.call.args;
        int num_args = args ? args->size : 0;
        int i, j;
        int total_words = 0;
        int stack_words_count = 0;
        int is_direct = (node->u.call.func->kind == AST_VAR && node->u.call.func->u.sym->kind == SYM_FUNC);
        int *arg_words = (int *)malloc((num_args > 0 ? num_args : 1) * sizeof(int));
        int *arg_pads = (int *)malloc((num_args > 0 ? num_args : 1) * sizeof(int));
        int off_fn = 0;

        for (i = 0; i < num_args; i++) {
            AstNode *arg = (AstNode *)vec_get(args, i);
            int words = 1;
            int pad = 0;
            if (arg->type && is_multiword_arg(arg->type)) {
                int sz = (arg->type->size + 3) & ~3;
                words = sz / 4;
                if (words < 1) words = 1;
            }
            if (needs_8byte_align(arg->type)) {
                if (total_words % 2 != 0) {
                    pad = 1;
                    total_words++;
                }
            }
            arg_pads[i] = pad;
            arg_words[i] = words;
            total_words += words;
        }

        if (total_words > 8) {
            stack_words_count = total_words - 8;
        }

        if (is_direct) {
            const char *fname = node->u.call.func->u.sym->name;
            if (strcmp(fname, "__builtin_expect") == 0) {
                if (args && args->size > 0) {
                    gen_expr(gen, (AstNode *)vec_get(args, 0));
                }
                free(arg_words);
                free(arg_pads);
                return;
            }
            if (strcmp(fname, "__builtin_constant_p") == 0) {
                emit(gen, "    li a0, 0");
                free(arg_words);
                free(arg_pads);
                return;
            }
            if (strcmp(fname, "__builtin_va_start") == 0) {
                if (args && args->size > 0) {
                    int num_params = gen->current_func && gen->current_func->type && gen->current_func->type->params ?
                                     gen->current_func->type->params->size : 1;
                    gen_lval(gen, (AstNode *)vec_get(args, 0));
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw a0, 0(sp)");
                    emit(gen, "    addi a0, s0, -%d", 32 - num_params * 4);
                    emit(gen, "    lw a1, 0(sp)");
                    emit(gen, "    sw a0, 0(a1)");
                    emit(gen, "    addi sp, sp, 16");
                }
                free(arg_words);
                free(arg_pads);
                return;
            }
            if (strcmp(fname, "__builtin_va_copy") == 0) {
                if (args && args->size >= 2) {
                    gen_expr(gen, (AstNode *)vec_get(args, 1));
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw a0, 0(sp)");
                    gen_lval(gen, (AstNode *)vec_get(args, 0));
                    emit(gen, "    lw a1, 0(sp)");
                    emit(gen, "    sw a1, 0(a0)");
                    emit(gen, "    addi sp, sp, 16");
                }
                free(arg_words);
                free(arg_pads);
                return;
            }
            if (strcmp(fname, "__builtin_va_end") == 0) {
                free(arg_words);
                free(arg_pads);
                return;
            }
        }

        if (!is_direct) {
            off_fn = get_scratch_temp(gen);
            gen_expr(gen, node->u.call.func);
            emit_sw_s0(gen, "a0", off_fn);
        }

        if (total_words <= 8) {
            /* Evaluate args forward, push each word (16-byte aligned per push) */
            for (i = 0; i < num_args; i++) {
                AstNode *arg = (AstNode *)vec_get(args, i);
                if (arg_pads[i]) {
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw zero, 0(sp)");
                }
                gen_expr(gen, arg);
                if (arg->type && is_multiword_arg(arg->type)) {
                    int w;
                    for (w = 0; w < arg_words[i]; w++) {
                        emit(gen, "    lw t0, %d(a0)", w * 4);
                        emit(gen, "    addi sp, sp, -16");
                        emit(gen, "    sw t0, 0(sp)");
                    }
                } else {
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw a0, 0(sp)");
                }
            }
            /* Pop into a{total_words-1} down to a0 */
            for (i = total_words - 1; i >= 0; i--) {
                emit(gen, "    lw a%d, 0(sp)", i);
                emit(gen, "    addi sp, sp, 16");
            }
            if (!is_direct) {
                emit_lw_s0(gen, "t0", off_fn);
            }
        } else {
            int stack_bytes = (stack_words_count * 4 + 15) & ~15;

            /* Evaluate args forward, push each word (16-byte aligned per push) */
            for (i = 0; i < num_args; i++) {
                AstNode *arg = (AstNode *)vec_get(args, i);
                if (arg_pads[i]) {
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw zero, 0(sp)");
                }
                gen_expr(gen, arg);
                if (arg->type && is_multiword_arg(arg->type)) {
                    int w;
                    for (w = 0; w < arg_words[i]; w++) {
                        emit(gen, "    lw t0, %d(a0)", w * 4);
                        emit(gen, "    addi sp, sp, -16");
                        emit(gen, "    sw t0, 0(sp)");
                    }
                } else {
                    emit(gen, "    addi sp, sp, -16");
                    emit(gen, "    sw a0, 0(sp)");
                }
            }

            /* Load register words 0..7 into a0..a7 from their 16-byte slots */
            for (i = 0; i < 8; i++) {
                int offset = (total_words - 1 - i) * 16;
                emit(gen, "    lw a%d, %d(sp)", i, offset);
            }

            /* Allocate 16-byte aligned packed stack argument frame */
            emit(gen, "    addi sp, sp, -%d", stack_bytes);

            /* Copy stack arguments (words 8..total_words-1) into packed slots at 0(sp), 4(sp)... */
            for (j = 0; j < stack_words_count; j++) {
                int src_off = stack_bytes + (stack_words_count - 1 - j) * 16;
                emit(gen, "    lw t1, %d(sp)", src_off);
                emit(gen, "    sw t1, %d(sp)", j * 4);
            }

            if (!is_direct) {
                emit_lw_s0(gen, "t0", off_fn);
            }
        }
        free(arg_words);
        free(arg_pads);

        if (is_direct) {
            emit(gen, "    call %s", node->u.call.func->u.sym->name);
        } else {
            emit(gen, "    jalr t0");
        }

        if (total_words > 8) {
            int stack_bytes = (stack_words_count * 4 + 15) & ~15;
            emit(gen, "    addi sp, sp, %d", total_words * 16 + stack_bytes);
        }

        if (node->type && node->type->kind == TYPE_DOUBLE) {
            int off = get_scratch_temp(gen);
            emit_addi(gen, "t0", "s0", off);
            emit(gen, "    sw a0, 0(t0)");
            emit(gen, "    sw a1, 4(t0)");
            emit_addi(gen, "a0", "s0", off);
        }
        return;
    }

    case AST_VA_ARG: {
        int size = node->type ? (node->type->size > 0 ? node->type->size : 4) : 4;
        int aligned_size = (size + 3) & ~3;
        int need_align8 = needs_8byte_align(node->type);
        if (aligned_size < 4) aligned_size = 4;
        if (need_align8) {
            /* Align ap to 8-byte boundary */
            if (node->u.va_arg.ap->kind == AST_VAR) {
                Symbol *sym = node->u.va_arg.ap->u.sym;
                emit_addi(gen, "a1", "s0", sym->stack_offset);
                emit(gen, "    lw t0, 0(a1)");
                emit(gen, "    addi t0, t0, 7");
                emit(gen, "    andi t0, t0, -8");
                emit(gen, "    sw t0, 0(a1)");
            } else {
                gen_lval(gen, node->u.va_arg.ap);
                emit(gen, "    lw t0, 0(a0)");
                emit(gen, "    addi t0, t0, 7");
                emit(gen, "    andi t0, t0, -8");
                emit(gen, "    sw t0, 0(a0)");
            }
        }
        gen_expr(gen, node->u.va_arg.ap);
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)");
        if (node->u.va_arg.ap->kind == AST_VAR) {
            Symbol *sym = node->u.va_arg.ap->u.sym;
            emit_addi(gen, "a1", "s0", sym->stack_offset);
            emit(gen, "    lw t0, 0(a1)");
            emit(gen, "    addi t0, t0, %d", aligned_size);
            emit(gen, "    sw t0, 0(a1)");
        } else {
            gen_lval(gen, node->u.va_arg.ap);
            emit(gen, "    lw t0, 0(a0)");
            emit(gen, "    addi t0, t0, %d", aligned_size);
            emit(gen, "    sw t0, 0(a0)");
        }
        emit(gen, "    lw a0, 0(sp)");
        emit(gen, "    addi sp, sp, 16");
        if (node->type && (node->type->kind == TYPE_STRUCT || node->type->kind == TYPE_UNION ||
                           node->type->kind == TYPE_DOUBLE || node->type->kind == TYPE_LDOUBLE)) {
            /* Keep a0 as pointer to the multi-word value in va_list buffer */
        } else if (size == 1) {
            if (node->type && node->type->is_unsigned) emit(gen, "    lbu a0, 0(a0)");
            else emit(gen, "    lb a0, 0(a0)");
        } else if (size == 2) {
            if (node->type && node->type->is_unsigned) emit(gen, "    lhu a0, 0(a0)");
            else emit(gen, "    lh a0, 0(a0)");
        } else {
            emit(gen, "    lw a0, 0(a0)");
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
            if (common_type->kind == TYPE_FLOAT) {
                gen_expr(gen, lhs);
                if (!type_equal(lhs->type, common_type)) {
                    gen_cast_to(gen, lhs->type, common_type);
                }
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    sw a0, 0(sp)");
                gen_expr(gen, rhs);
                if (!type_equal(rhs->type, common_type)) {
                    gen_cast_to(gen, rhs->type, common_type);
                }
                emit(gen, "    mv a1, a0"); /* rhs in a1 */
                emit(gen, "    lw a0, 0(sp)"); /* lhs in a0 */
                emit(gen, "    addi sp, sp, 16");

                switch (node->kind) {
                case AST_ADD: emit(gen, "    call __addsf3"); break;
                case AST_SUB: emit(gen, "    call __subsf3"); break;
                case AST_MUL: emit(gen, "    call __mulsf3"); break;
                case AST_DIV: emit(gen, "    call __divsf3"); break;
                case AST_EQ:
                    emit(gen, "    call __eqsf2");
                    emit(gen, "    seqz a0, a0");
                    break;
                case AST_NE:
                    emit(gen, "    call __nesf2");
                    emit(gen, "    snez a0, a0");
                    break;
                case AST_LT:
                    emit(gen, "    call __ltsf2");
                    emit(gen, "    slti a0, a0, 0");
                    break;
                case AST_LE:
                    emit(gen, "    call __lesf2");
                    emit(gen, "    slti a0, a0, 1");
                    break;
                case AST_GT:
                    emit(gen, "    call __gtsf2");
                    emit(gen, "    slt a0, zero, a0");
                    break;
                case AST_GE:
                    emit(gen, "    call __gesf2");
                    emit(gen, "    slt a0, a0, zero");
                    emit(gen, "    xori a0, a0, 1");
                    break;
                default: break;
                }
            } else if (common_type->kind == TYPE_DOUBLE) {
                gen_expr(gen, lhs);
                if (!type_equal(lhs->type, common_type)) {
                    gen_cast_to(gen, lhs->type, common_type);
                }
                /* Push the FULL 8-BYTE VALUE of lhs to the stack so rhs cannot clobber it */
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    lw t0, 0(a0)");
                emit(gen, "    lw t1, 4(a0)");
                emit(gen, "    sw t0, 0(sp)");
                emit(gen, "    sw t1, 4(sp)");
                gen_expr(gen, rhs);
                if (!type_equal(rhs->type, common_type)) {
                    gen_cast_to(gen, rhs->type, common_type);
                }
                /* Load rhs (8 bytes) into a2:a3 */
                emit(gen, "    lw a2, 0(a0)");
                emit(gen, "    lw a3, 4(a0)");
                /* Load lhs (8 bytes) from stack into a0:a1 */
                emit(gen, "    lw a0, 0(sp)");
                emit(gen, "    lw a1, 4(sp)");
                emit(gen, "    addi sp, sp, 16");

                if (node->kind == AST_EQ || node->kind == AST_NE || node->kind == AST_LT ||
                    node->kind == AST_LE || node->kind == AST_GT || node->kind == AST_GE) {
                    switch (node->kind) {
                    case AST_EQ:
                        emit(gen, "    call __eqdf2");
                        emit(gen, "    seqz a0, a0");
                        break;
                    case AST_NE:
                        emit(gen, "    call __nedf2");
                        emit(gen, "    snez a0, a0");
                        break;
                    case AST_LT:
                        emit(gen, "    call __ltdf2");
                        emit(gen, "    slti a0, a0, 0");
                        break;
                    case AST_LE:
                        emit(gen, "    call __ledf2");
                        emit(gen, "    slti a0, a0, 1");
                        break;
                    case AST_GT:
                        emit(gen, "    call __gtdf2");
                        emit(gen, "    slt a0, zero, a0");
                        break;
                    case AST_GE:
                        emit(gen, "    call __gedf2");
                        emit(gen, "    slt a0, a0, zero");
                        emit(gen, "    xori a0, a0, 1");
                        break;
                    default: break;
                    }
                } else {
                    int off = get_scratch_temp(gen);
                    if (node->kind == AST_ADD) emit(gen, "    call __adddf3");
                    else if (node->kind == AST_SUB) emit(gen, "    call __subdf3");
                    else if (node->kind == AST_MUL) emit(gen, "    call __muldf3");
                    else if (node->kind == AST_DIV) emit(gen, "    call __divdf3");
                    emit_addi(gen, "t0", "s0", off);
                    emit(gen, "    sw a0, 0(t0)");
                    emit(gen, "    sw a1, 4(t0)");
                    emit_addi(gen, "a0", "s0", off);
                }
            } else if (common_type->kind == TYPE_LDOUBLE) {
                gen_expr(gen, lhs);
                if (!type_equal(lhs->type, common_type)) {
                    gen_cast_to(gen, lhs->type, common_type);
                }
                /* Push the FULL 16-BYTE VALUE of lhs to the stack */
                emit(gen, "    addi sp, sp, -16");
                emit(gen, "    lw t0, 0(a0)");
                emit(gen, "    lw t1, 4(a0)");
                emit(gen, "    sw t0, 0(sp)");
                emit(gen, "    sw t1, 4(sp)");
                emit(gen, "    lw t0, 8(a0)");
                emit(gen, "    lw t1, 12(a0)");
                emit(gen, "    sw t0, 8(sp)");
                emit(gen, "    sw t1, 12(sp)");
                gen_expr(gen, rhs);
                if (!type_equal(rhs->type, common_type)) {
                    gen_cast_to(gen, rhs->type, common_type);
                }
                /* rhs address is in a0, lhs address is at 0(sp) */
                emit(gen, "    mv a2, a0");      /* rhs */
                emit(gen, "    mv a1, sp");      /* lhs */
                if (node->kind == AST_EQ || node->kind == AST_NE || node->kind == AST_LT ||
                    node->kind == AST_LE || node->kind == AST_GT || node->kind == AST_GE) {
                    emit(gen, "    mv a0, a1");  /* lhs in a0 */
                    emit(gen, "    mv a1, a2");  /* rhs in a1 */
                    switch (node->kind) {
                    case AST_EQ:
                        emit(gen, "    call __eqtf2");
                        emit(gen, "    seqz a0, a0");
                        break;
                    case AST_NE:
                        emit(gen, "    call __netf2");
                        emit(gen, "    snez a0, a0");
                        break;
                    case AST_LT:
                        emit(gen, "    call __lttf2");
                        emit(gen, "    slti a0, a0, 0");
                        break;
                    case AST_LE:
                        emit(gen, "    call __letf2");
                        emit(gen, "    slti a0, a0, 1");
                        break;
                    case AST_GT:
                        emit(gen, "    call __gttf2");
                        emit(gen, "    slt a0, zero, a0");
                        break;
                    case AST_GE:
                        emit(gen, "    call __getf2");
                        emit(gen, "    slt a0, a0, zero");
                        emit(gen, "    xori a0, a0, 1");
                        break;
                    default: break;
                    }
                    emit(gen, "    addi sp, sp, 16");
                } else {
                    int off = get_scratch_temp(gen);
                    emit_addi(gen, "a0", "s0", off);
                    if (node->kind == AST_ADD) emit(gen, "    call __addtf3");
                    else if (node->kind == AST_SUB) emit(gen, "    call __subtf3");
                    else if (node->kind == AST_MUL) emit(gen, "    call __multf3");
                    else if (node->kind == AST_DIV) emit(gen, "    call __divtf3");
                    emit(gen, "    addi sp, sp, 16");
                    emit_addi(gen, "a0", "s0", off);
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
        emit(gen, "    addi sp, sp, -16");
        emit(gen, "    sw a0, 0(sp)");
        gen_expr(gen, rhs);
        emit(gen, "    mv a1, a0");
        emit(gen, "    lw a0, 0(sp)");
        emit(gen, "    addi sp, sp, 16");

        switch (node->kind) {
            case AST_ADD:
                if (is_ptr_lhs && !is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    if (scale > 1) {
                        emit(gen, "    addi sp, sp, -16");
                        emit(gen, "    sw a0, 0(sp)");
                        emit(gen, "    mv a0, a1");
                        emit(gen, "    li a1, %d", scale);
                        emit(gen, "    call __mulsi3");
                        emit(gen, "    mv a1, a0");
                        emit(gen, "    lw a0, 0(sp)");
                        emit(gen, "    addi sp, sp, 16");
                    }
                } else if (!is_ptr_lhs && is_ptr_rhs) {
                    int scale = rhs->type->base ? rhs->type->base->size : 1;
                    if (scale > 1) {
                        emit(gen, "    addi sp, sp, -16");
                        emit(gen, "    sw a1, 0(sp)");
                        emit(gen, "    li a1, %d", scale);
                        emit(gen, "    call __mulsi3");
                        emit(gen, "    lw a1, 0(sp)");
                        emit(gen, "    addi sp, sp, 16");
                    }
                }
                emit(gen, "    add a0, a0, a1");
                break;

            case AST_SUB:
                if (is_ptr_lhs && !is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    if (scale > 1) {
                        emit(gen, "    addi sp, sp, -16");
                        emit(gen, "    sw a0, 0(sp)");
                        emit(gen, "    mv a0, a1");
                        emit(gen, "    li a1, %d", scale);
                        emit(gen, "    call __mulsi3");
                        emit(gen, "    mv a1, a0");
                        emit(gen, "    lw a0, 0(sp)");
                        emit(gen, "    addi sp, sp, 16");
                    }
                    emit(gen, "    sub a0, a0, a1");
                } else if (is_ptr_lhs && is_ptr_rhs) {
                    int scale = lhs->type->base ? lhs->type->base->size : 1;
                    emit(gen, "    sub a0, a0, a1");
                    if (scale > 1) {
                        emit(gen, "    li a1, %d", scale);
                        emit(gen, "    call __divsi3");
                    }
                } else {
                    emit(gen, "    sub a0, a0, a1");
                }
                break;

            case AST_MUL:
                emit(gen, "    call __mulsi3");
                break;

            case AST_DIV:
                if (is_unsigned) emit(gen, "    call __udivsi3");
                else emit(gen, "    call __divsi3");
                break;

            case AST_MOD:
                if (is_unsigned) emit(gen, "    call __umodsi3");
                else emit(gen, "    call __modsi3");
                break;

            case AST_SHL:
                emit(gen, "    sll a0, a0, a1");
                break;

            case AST_SHR:
                if (is_unsigned) emit(gen, "    srl a0, a0, a1");
                else emit(gen, "    sra a0, a0, a1");
                break;

            case AST_BITAND:
                emit(gen, "    and a0, a0, a1");
                break;

            case AST_BITOR:
                emit(gen, "    or a0, a0, a1");
                break;

            case AST_BITXOR:
                emit(gen, "    xor a0, a0, a1");
                break;

            case AST_EQ:
                emit(gen, "    sub a0, a0, a1");
                emit(gen, "    seqz a0, a0");
                break;

            case AST_NE:
                emit(gen, "    sub a0, a0, a1");
                emit(gen, "    snez a0, a0");
                break;

            case AST_LT:
                if (is_unsigned) emit(gen, "    sltu a0, a0, a1");
                else emit(gen, "    slt a0, a0, a1");
                break;

            case AST_LE:
                if (is_unsigned) {
                    emit(gen, "    sltu a0, a1, a0");
                    emit(gen, "    xori a0, a0, 1");
                } else {
                    emit(gen, "    slt a0, a1, a0");
                    emit(gen, "    xori a0, a0, 1");
                }
                break;

            case AST_GT:
                if (is_unsigned) emit(gen, "    sltu a0, a1, a0");
                else emit(gen, "    slt a0, a1, a0");
                break;

            case AST_GE:
                if (is_unsigned) {
                    emit(gen, "    sltu a0, a0, a1");
                    emit(gen, "    xori a0, a0, 1");
                } else {
                    emit(gen, "    slt a0, a0, a1");
                    emit(gen, "    xori a0, a0, 1");
                }
                break;

            default:
                break;
        }
    }
}

/* ========================================================================= */
/* Statement Code Generation (32-bit RISC-V RV32I)                           */
/* ========================================================================= */

static void gen_local_initializer(CodeGen *gen, Symbol *sym, Type *type, Initializer *init, int base_offset) {
    if (!init) return;

    if (init->is_compound) {
        int i;
        if (type && type->kind == TYPE_STRUCT) {
            Member *m = type->members;
            for (i = 0; i < init->elements->size && m; i++, m = m->next) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                if (m->bit_width > 0) {
                    if (elem->expr) {
                        gen_expr(gen, elem->expr);
                        emit_addi(gen, "a1", "s0", sym->stack_offset + base_offset + m->offset);
                        emit(gen, "    addi sp, sp, -4");
                        emit(gen, "    sw a1, 0(sp)");
                        gen_bitfield_store(gen, m);
                    }
                } else {
                    gen_local_initializer(gen, sym, m->type, elem, base_offset + m->offset);
                }
            }
            return;
        }
        if (type && type->kind == TYPE_ARRAY) {
            Type *elem_type = type->base ? type->base : type_int;
            int elem_size = elem_type ? elem_type->size : 4;
            for (i = 0; i < init->elements->size; i++) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                gen_local_initializer(gen, sym, elem_type, elem, base_offset + i * elem_size);
            }
            return;
        }
        for (i = 0; i < init->elements->size; i++) {
            Initializer *elem = (Initializer *)vec_get(init->elements, i);
            gen_local_initializer(gen, sym, type->base ? type->base : type_int, elem, base_offset + i * 4);
        }
    } else if (init->expr) {
        Type *target_type;
        if (type && type->kind == TYPE_ARRAY && init->expr->kind == AST_STR_LIT) {
            /* Copy string literal characters into array on stack and zero rest */
            const char *str = init->expr->u.str_val.str;
            int len = init->expr->u.str_val.len;
            int total_size = type->size > 0 ? type->size : (len + 1);
            int i;
            emit_addi(gen, "a1", "s0", sym->stack_offset + base_offset);
            for (i = 0; i < total_size; i++) {
                char ch = (i < len) ? str[i] : 0;
                emit(gen, "    li t0, %d", (unsigned char)ch);
                if (i >= -2048 && i <= 2047) {
                    emit(gen, "    sb t0, %d(a1)", i);
                } else {
                    emit_addi(gen, "t1", "a1", i);
                    emit(gen, "    sb t0, 0(t1)");
                }
            }
            return;
        }

        target_type = (type && type->kind == TYPE_ARRAY) ? type->base : type;
        gen_expr(gen, init->expr);
        emit_addi(gen, "a1", "s0", sym->stack_offset + base_offset);
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
        emit(gen, "    beqz a0, %s", node->u.if_stmt.else_stmt ? label_else : label_end);
        gen_stmt(gen, node->u.if_stmt.then_stmt);
        if (node->u.if_stmt.else_stmt) {
            emit(gen, "    j %s", label_end);
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
        emit(gen, "    beqz a0, %s", node->u.loop_stmt.break_label);
        gen_stmt(gen, node->u.loop_stmt.body);
        emit(gen, "    j %s", label_start);
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
        emit(gen, "    bnez a0, %s", label_start);
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
            emit(gen, "    beqz a0, %s", node->u.for_stmt.break_label);
        }
        gen_stmt(gen, node->u.for_stmt.body);
        emit(gen, "%s:", node->u.for_stmt.continue_label);
        if (node->u.for_stmt.step) {
            gen_expr(gen, node->u.for_stmt.step);
        }
        emit(gen, "    j %s", label_start);
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
                emit(gen, "    li a1, %d", (int)cnode->u.case_stmt.val);
                emit(gen, "    beq a0, a1, %s", cnode->u.case_stmt.label);
            }
        }
        if (node->u.switch_stmt.default_label) {
            emit(gen, "    j %s", node->u.switch_stmt.default_label);
        } else {
            emit(gen, "    j %s", node->u.switch_stmt.break_label);
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
            emit(gen, "    j %s", lbl);
        }
        break;

    case AST_CONTINUE:
        if (gen->continue_stack->size > 0) {
            char *lbl = (char *)vec_get(gen->continue_stack, gen->continue_stack->size - 1);
            emit(gen, "    j %s", lbl);
        }
        break;

    case AST_RETURN:
        if (node->u.return_stmt.expr) {
            gen_expr(gen, node->u.return_stmt.expr);
            if (gen->current_func && gen->current_func->type && gen->current_func->type->base) {
                Type *ret_type = gen->current_func->type->base;
                if (!type_equal(node->u.return_stmt.expr->type, ret_type)) {
                    gen_cast_to(gen, node->u.return_stmt.expr->type, ret_type);
                }
                if (ret_type->kind == TYPE_DOUBLE) {
                    emit(gen, "    lw a1, 4(a0)");
                    emit(gen, "    lw a0, 0(a0)");
                }
            }
        }
        if (gen->func_ret_label) {
            emit(gen, "    j %s", gen->func_ret_label);
        }
        break;

    case AST_GOTO:
        emit(gen, "    j .L_user_%s", node->u.label_stmt.name);
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
/* Global Variables & Data Sections (32-bit RISC-V RV32I)                    */
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
            for (i = 0; i < init->elements->size && m; ) {
                if (m->bit_width > 0) {
                    int container_offset = m->offset;
                    Type *container_type = m->type;
                    unsigned long packed_val = 0;
                    if (container_offset > cur_offset) {
                        emit(gen, "    .zero %d", container_offset - cur_offset);
                        cur_offset = container_offset;
                    }
                    while (i < init->elements->size && m && m->bit_width > 0 && m->offset == container_offset) {
                        Initializer *elem = (Initializer *)vec_get(init->elements, i);
                        long val = 0;
                        if (elem->expr) val = eval_const_expr(elem->expr);
                        if (m->bit_width < 32) {
                            unsigned int mask = (1U << m->bit_width) - 1U;
                            packed_val |= ((unsigned long)val & mask) << m->bit_offset;
                        } else {
                            packed_val |= (unsigned long)val;
                        }
                        i++;
                        m = m->next;
                    }
                    if (container_type->size == 1) emit(gen, "    .byte %u", (unsigned int)(packed_val & 0xFF));
                    else if (container_type->size == 2) emit(gen, "    .value %u", (unsigned int)(packed_val & 0xFFFF));
                    else emit(gen, "    .long %u", (unsigned int)(packed_val & 0xFFFFFFFFU));
                    cur_offset += (container_type->size > 0 ? container_type->size : 4);
                } else {
                    Initializer *elem = (Initializer *)vec_get(init->elements, i);
                    if (m->offset > cur_offset) {
                        emit(gen, "    .zero %d", m->offset - cur_offset);
                        cur_offset = m->offset;
                    }
                    gen_global_init(gen, elem, m->type);
                    cur_offset += (m->type && m->type->size > 0) ? m->type->size : 4;
                    i++;
                    m = m->next;
                }
            }
            if (cur_offset < type->size) {
                emit(gen, "    .zero %d", type->size - cur_offset);
            }
            return;
        } else if (type && type->kind == TYPE_ARRAY) {
            Type *elem_type = type->base ? type->base : type_int;
            int elem_size = elem_type ? (elem_type->size > 0 ? elem_type->size : 4) : 4;
            int total_emitted = 0;
            for (i = 0; i < init->elements->size; i++) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                gen_global_init(gen, elem, elem_type);
                total_emitted += elem_size;
            }
            if (type->size > total_emitted) {
                emit(gen, "    .zero %d", type->size - total_emitted);
            }
            return;
        } else {
            for (i = 0; i < init->elements->size; i++) {
                Initializer *elem = (Initializer *)vec_get(init->elements, i);
                gen_global_init(gen, elem, type->base ? type->base : type_int);
            }
            return;
        }
    } else if (init->expr) {
        AstNode *e = init->expr;
        while (e && e->kind == AST_CAST) e = e->u.cast.operand;
        if (!e) {
            emit(gen, "    .zero %d", type->size);
            return;
        }
        if (type && type->kind == TYPE_ARRAY && type->base && type->base->kind == TYPE_CHAR && e->kind == AST_STR_LIT) {
            /* Copy string literal characters into array */
            int len = e->u.str_val.len + 1;
            int j;
            for (j = 0; j < e->u.str_val.len; j++) {
                emit(gen, "    .byte %d", (unsigned char)e->u.str_val.str[j]);
            }
            emit(gen, "    .byte 0");
            if (type->size > len) {
                emit(gen, "    .zero %d", type->size - len);
            }
            return;
        }
        if (type && type_is_floating(type)) {
            unsigned int words[4];
            words[0] = words[1] = words[2] = words[3] = 0;
            eval_const_float_expr(e, words, type);
            if (type->kind == TYPE_FLOAT) {
                emit(gen, "    .long %u", words[0]);
            } else if (type->kind == TYPE_LDOUBLE) {
                emit(gen, "    .long %u", words[0]);
                emit(gen, "    .long %u", words[1]);
                emit(gen, "    .long %u", words[2]);
                emit(gen, "    .long %u", words[3]);
            } else {
                emit(gen, "    .long %u", words[0]);
                emit(gen, "    .long %u", words[1]);
            }
            return;
        }
        if (e->kind == AST_INT_LIT || e->kind == AST_CHAR_LIT) {
            if (type->size == 1) emit(gen, "    .byte %ld", e->u.int_val.val);
            else if (type->size == 2) emit(gen, "    .value %ld", e->u.int_val.val);
            else if (type->size == 4) emit(gen, "    .long %ld", e->u.int_val.val);
            else emit(gen, "    .quad %ld", e->u.int_val.val);
        } else if (e->kind == AST_STR_LIT) {
            emit(gen, "    .long %s", e->u.str_val.label);
        } else if (e->kind == AST_VAR) {
            const char *name = e->u.sym->asm_label ? e->u.sym->asm_label : e->u.sym->name;
            emit(gen, "    .long %s", name);
        } else if (e->kind == AST_ADDR && e->u.unop.operand->kind == AST_VAR) {
            const char *name = e->u.unop.operand->u.sym->asm_label ? e->u.unop.operand->u.sym->asm_label : e->u.unop.operand->u.sym->name;
            emit(gen, "    .long %s", name);
        } else {
            long val = eval_const_expr(e);
            if (type->size == 1) emit(gen, "    .byte %ld", val);
            else if (type->size == 2) emit(gen, "    .value %ld", val);
            else if (type->size == 4) emit(gen, "    .long %ld", val);
            else emit(gen, "    .quad %ld", val);
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
            emit(gen, "    .balign %d", sym->type->align > 0 ? sym->type->align : 4);
            emit(gen, "%s:", gname);
            gen_global_init(gen, sym->init, sym->type);
        } else {
            emit(gen, "    .bss");
            emit(gen, "    .balign %d", sym->type->align > 0 ? sym->type->align : 4);
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
            if (f->type == type_float) {
                emit(gen, "    .balign 4");
                emit(gen, "%s:", f->u.float_val.label);
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
            } else if (f->type == type_ldouble) {
                emit(gen, "    .balign 16");
                emit(gen, "%s:", f->u.float_val.label);
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[1]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[2]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[3]);
            } else {
                emit(gen, "    .balign 8");
                emit(gen, "%s:", f->u.float_val.label);
                emit(gen, "    .long %u", f->u.float_val.u128_words[0]);
                emit(gen, "    .long %u", f->u.float_val.u128_words[1]);
            }
        }
    }
}

/* ========================================================================= */
/* Function Definition Code Generation (32-bit RISC-V RV32I)                 */
/* ========================================================================= */

static void gen_func_def(CodeGen *gen, AstNode *func_node) {
    Symbol *sym = func_node->u.func_def.sym;
    int stack_size = func_node->u.func_def.stack_size;
    int i;

    gen->func_ret_label = gen_asm_label(gen, "ret");
    gen->current_func = sym;
    gen->current_stack_size = stack_size;

    emit(gen, "    .text");
    if (sym->storage != STORAGE_STATIC) {
        emit(gen, "    .globl %s", sym->name);
    }
    emit(gen, "    .type %s, @function", sym->name);
    emit(gen, "%s:", sym->name);

    /* Function prologue (16-byte aligned stack frame) */
    emit_addi(gen, "sp", "sp", -stack_size);
    emit(gen, "    sw ra, 0(sp)");
    emit(gen, "    sw s0, 4(sp)");
    emit_addi(gen, "s0", "sp", stack_size);

    /* Spill incoming arguments to their local stack slots */
    {
        Vector *params = func_node->u.func_def.params;
        if (params) {
            int arg_reg = 0;
            int stack_arg_offset = 0;
            for (i = 0; i < params->size; i++) {
                Symbol *psym = (Symbol *)vec_get(params, i);
                if (psym) {
                    int psize = (psym->type->size + 3) & ~3;
                    int words = psize / 4;
                    int w;
                    if (words < 1) words = 1;
                    if (needs_8byte_align(psym->type)) {
                        if (arg_reg < 8 && (arg_reg % 2 != 0)) {
                            arg_reg++;
                        } else if (arg_reg >= 8 && (stack_arg_offset % 8 != 0)) {
                            stack_arg_offset += 4;
                        }
                    }
                    for (w = 0; w < words; w++) {
                        int slot_offset = psym->stack_offset + w * 4;
                        if (arg_reg < 8) {
                            if (slot_offset >= -2048 && slot_offset <= 2047) {
                                emit(gen, "    sw a%d, %d(s0)", arg_reg, slot_offset);
                            } else {
                                emit_addi(gen, "t0", "s0", slot_offset);
                                emit(gen, "    sw a%d, 0(t0)", arg_reg);
                            }
                            arg_reg++;
                        } else {
                            /* Passed on stack by caller at stack_arg_offset(s0) */
                            emit_addi(gen, "t1", "s0", stack_arg_offset);
                            emit(gen, "    lw t0, 0(t1)");
                            emit_addi(gen, "t1", "s0", slot_offset);
                            emit(gen, "    sw t0, 0(t1)");
                            stack_arg_offset += 4;
                        }
                    }
                }
            }
        }
    }

    /* Spill variadic arguments for va_list */
    if (sym->type && sym->type->is_varargs) {
        for (i = 0; i < 8; i++) {
            emit(gen, "    sw a%d, -%d(s0)", i, 32 - i * 4);
        }
    }

    /* Generate function body statements */
    gen_stmt(gen, func_node->u.func_def.body);

    /* Function Epilogue */
    emit(gen, "%s:", gen->func_ret_label);
    emit_addi(gen, "sp", "s0", -stack_size);
    emit(gen, "    lw ra, 0(sp)");
    emit(gen, "    lw s0, 4(sp)");
    emit_addi(gen, "sp", "sp", stack_size);
    emit(gen, "    ret");

    free(gen->func_ret_label);
    gen->func_ret_label = NULL;
    gen->current_func = NULL;
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
