/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#include <string.h>
#include "../include/softfloat.h"

/* ========================================================================= */
/* Type Unions for Bit Representation                                        */
/* ========================================================================= */

typedef union {
    float f;
    unsigned int u;
} f32_cast;

typedef union {
    double d;
    unsigned long u64;
    struct {
        unsigned int lo;
        unsigned int hi;
    } u;
} f64_cast;

#if !defined(TARGET_I386) && !defined(__i386__) && !defined(TARGET_RISCV32) && !defined(__riscv) && !defined(TARGET_32BIT)
static void f64_from_ulong(f64_cast *c, unsigned long val) {
    c->u64 = val;
}

static unsigned long f64_to_ulong(const f64_cast *c) {
    return c->u64;
}
#endif

/* ========================================================================= */
/* Pure ANSI C90 Multi-Word Integer Helper Operations                        */
/* ========================================================================= */

static void mul32x32_to_64(unsigned int *res_hi, unsigned int *res_lo, unsigned int u, unsigned int v) {
    unsigned long u_lo = u & 0xFFFFU;
    unsigned long u_hi = u >> 16;
    unsigned long v_lo = v & 0xFFFFU;
    unsigned long v_hi = v >> 16;
    unsigned long p0 = u_lo * v_lo;
    unsigned long p1 = u_lo * v_hi;
    unsigned long p2 = u_hi * v_lo;
    unsigned long p3 = u_hi * v_hi;
    unsigned long mid = p1 + p2 + (p0 >> 16);
    *res_lo = (unsigned int)((p0 & 0xFFFFU) | ((mid & 0xFFFFU) << 16));
    *res_hi = (unsigned int)(p3 + (mid >> 16));
}

static void u128_zero(soft_f128 *a) {
    a->w[0] = 0;
    a->w[1] = 0;
    a->w[2] = 0;
    a->w[3] = 0;
}

static int u128_is_zero(const soft_f128 *a) {
    return (a->w[0] | a->w[1] | a->w[2] | a->w[3]) == 0;
}

static int u128_cmp(const soft_f128 *a, const soft_f128 *b) {
    int i;
    for (i = 3; i >= 0; i--) {
        if (a->w[i] < b->w[i]) return -1;
        if (a->w[i] > b->w[i]) return 1;
    }
    return 0;
}

static unsigned int u128_add(soft_f128 *res, const soft_f128 *a, const soft_f128 *b) {
    unsigned int carry = 0;
    int i;
    for (i = 0; i < 4; i++) {
        unsigned int ai = a->w[i];
        unsigned int bi = b->w[i];
        unsigned int sum = ai + bi + carry;
        res->w[i] = sum;
        if (carry) {
            carry = (sum <= ai) ? 1 : 0;
        } else {
            carry = (sum < ai) ? 1 : 0;
        }
    }
    return carry;
}

static unsigned int u128_sub(soft_f128 *res, const soft_f128 *a, const soft_f128 *b) {
    unsigned int borrow = 0;
    int i;
    for (i = 0; i < 4; i++) {
        unsigned int ai = a->w[i];
        unsigned int bi = b->w[i];
        unsigned int diff = ai - bi - borrow;
        res->w[i] = diff;
        if (borrow) {
            borrow = (ai <= bi) ? 1 : 0;
        } else {
            borrow = (ai < bi) ? 1 : 0;
        }
    }
    return borrow;
}

static void u128_shl(soft_f128 *res, const soft_f128 *a, int count) {
    int word_shift, bit_shift, i;
    soft_f128 tmp;
    if (count <= 0) {
        if (res != a) *res = *a;
        return;
    }
    if (count >= 128) {
        u128_zero(res);
        return;
    }
    u128_zero(&tmp);
    word_shift = count / 32;
    bit_shift = count % 32;

    for (i = 3; i >= word_shift; i--) {
        tmp.w[i] = a->w[i - word_shift] << bit_shift;
        if (bit_shift > 0 && i - word_shift - 1 >= 0) {
            tmp.w[i] |= (a->w[i - word_shift - 1] >> (32 - bit_shift));
        }
    }
    *res = tmp;
}

static void u128_shr(soft_f128 *res, const soft_f128 *a, int count) {
    int word_shift, bit_shift, i;
    soft_f128 tmp;
    if (count <= 0) {
        if (res != a) *res = *a;
        return;
    }
    if (count >= 128) {
        u128_zero(res);
        return;
    }
    u128_zero(&tmp);
    word_shift = count / 32;
    bit_shift = count % 32;

    for (i = 0; i <= 3 - word_shift; i++) {
        tmp.w[i] = a->w[i + word_shift] >> bit_shift;
        if (bit_shift > 0 && i + word_shift + 1 < 4) {
            tmp.w[i] |= (a->w[i + word_shift + 1] << (32 - bit_shift));
        }
    }
    *res = tmp;
}

static void u128_div(soft_f128 *quot, soft_f128 *rem, const soft_f128 *num, const soft_f128 *den) {
    soft_f128 q, r;
    int i;
    u128_zero(&q);
    u128_zero(&r);
    for (i = 127; i >= 0; i--) {
        int bit = (num->w[i / 32] >> (i % 32)) & 1;
        u128_shl(&r, &r, 1);
        r.w[0] |= (unsigned int)bit;
        if (u128_cmp(&r, den) >= 0) {
            u128_sub(&r, &r, den);
            q.w[i / 32] |= (1U << (i % 32));
        }
    }
    if (quot) *quot = q;
    if (rem) *rem = r;
}

/* ========================================================================= */
/* Float32 (Single Precision IEEE 754) Implementation                        */
/* ========================================================================= */

#define F32_SIGN_MASK 0x80000000U
#define F32_EXP_MASK  0x7F800000U
#define F32_MANT_MASK 0x007FFFFFU
#define F32_BIAS      127

static void f32_unpack(unsigned int u, int *sign, int *exp, unsigned long *mant) {
    *sign = (u & F32_SIGN_MASK) ? 1 : 0;
    *exp = (int)((u & F32_EXP_MASK) >> 23);
    *mant = u & F32_MANT_MASK;
    if (*exp == 0) {
        if (*mant != 0) *exp = 1;
    } else if (*exp < 255) {
        *mant |= 0x00800000UL; /* implicit 1 */
    }
}

static unsigned int f32_pack(int sign, int exp, unsigned long mant) {
    if (exp <= 0) {
        if (exp < -24) return (unsigned int)(sign ? F32_SIGN_MASK : 0);
        mant >>= (1 - exp);
        exp = 0;
    } else if (exp >= 255) {
        return (unsigned int)((sign ? F32_SIGN_MASK : 0) | F32_EXP_MASK);
    }
    return (unsigned int)(((unsigned long)(sign ? 1 : 0) << 31) |
                          (((unsigned long)exp & 0xFFUL) << 23) |
                          (mant & F32_MANT_MASK));
}

unsigned int __addsf3(unsigned int a, unsigned int b) {
    int sa, sb, ea, eb, er;
    unsigned long ma, mb, mr;
    f32_unpack(a, &sa, &ea, &ma);
    f32_unpack(b, &sb, &eb, &mb);

    if (ma == 0) return b;
    if (mb == 0) return a;

    if (ea > eb) {
        int diff = ea - eb;
        if (diff > 31) mb = 0; else mb >>= diff;
        er = ea;
    } else {
        int diff = eb - ea;
        if (diff > 31) ma = 0; else ma >>= diff;
        er = eb;
    }

    if (sa == sb) {
        mr = ma + mb;
        if (mr & 0x01000000UL) {
            mr >>= 1;
            er++;
        }
        return f32_pack(sa, er, mr);
    } else {
        int sr;
        if (ma >= mb) {
            mr = ma - mb;
            sr = sa;
        } else {
            mr = mb - ma;
            sr = sb;
        }
        if (mr == 0) return 0;
        while ((mr & 0x00800000UL) == 0 && er > 0) {
            mr <<= 1;
            er--;
        }
        return f32_pack(sr, er, mr);
    }
}

unsigned int __subsf3(unsigned int a, unsigned int b) {
    return __addsf3(a, b ^ F32_SIGN_MASK);
}

unsigned int __mulsf3(unsigned int a, unsigned int b) {
    int sa, sb, ea, eb, er;
    unsigned long ma, mb, mr;
    unsigned int p_hi, p_lo;
    f32_unpack(a, &sa, &ea, &ma);
    f32_unpack(b, &sb, &eb, &mb);

    if (ma == 0 || mb == 0) return (unsigned int)((sa ^ sb) ? F32_SIGN_MASK : 0);

    er = ea + eb - F32_BIAS;
    mul32x32_to_64(&p_hi, &p_lo, (unsigned int)ma, (unsigned int)mb);
    mr = ((unsigned long)p_hi << 9) | ((unsigned long)p_lo >> 23);

    if (mr & 0x01000000UL) {
        mr >>= 1;
        er++;
    }
    while ((mr & 0x00800000UL) == 0 && er > 0) {
        mr <<= 1;
        er--;
    }
    return f32_pack(sa ^ sb, er, mr);
}

unsigned int __divsf3(unsigned int a, unsigned int b) {
    int sa, sb, ea, eb, er;
    unsigned long ma, mb;
    soft_f128 num, den, quot;
    f32_unpack(a, &sa, &ea, &ma);
    f32_unpack(b, &sb, &eb, &mb);

    if (mb == 0) return (unsigned int)((sa ^ sb) ? (F32_SIGN_MASK | F32_EXP_MASK) : F32_EXP_MASK);
    if (ma == 0) return (unsigned int)((sa ^ sb) ? F32_SIGN_MASK : 0);

    er = ea - eb + F32_BIAS;
    u128_zero(&num);
    u128_zero(&den);
    num.w[0] = (unsigned int)ma;
    u128_shl(&num, &num, 23);

    den.w[0] = (unsigned int)mb;
    u128_div(&quot, NULL, &num, &den);
    {
        unsigned long mr = quot.w[0];
        while (mr && (mr & 0x00800000UL) == 0 && er > 0) {
            mr <<= 1;
            er--;
        }
        if (mr & 0x01000000UL) {
            mr >>= 1;
            er++;
        }
        return f32_pack(sa ^ sb, er, mr);
    }
}

unsigned int __negsf2(unsigned int a) {
    return a ^ F32_SIGN_MASK;
}

int __eqsf2(unsigned int a, unsigned int b) {
    if ((a & ~F32_SIGN_MASK) == 0 && (b & ~F32_SIGN_MASK) == 0) return 0;
    return (a == b) ? 0 : 1;
}

int __nesf2(unsigned int a, unsigned int b) {
    return __eqsf2(a, b);
}

int __ltsf2(unsigned int a, unsigned int b) {
    int sa = (a & F32_SIGN_MASK) ? 1 : 0;
    int sb = (b & F32_SIGN_MASK) ? 1 : 0;
    if ((a & ~F32_SIGN_MASK) == 0 && (b & ~F32_SIGN_MASK) == 0) return 0;
    if (sa != sb) return sa ? -1 : 1;
    if (sa) return (a > b) ? -1 : (a < b ? 1 : 0);
    return (a < b) ? -1 : (a > b ? 1 : 0);
}

int __lesf2(unsigned int a, unsigned int b) {
    int c = __ltsf2(a, b);
    return (c <= 0) ? 0 : 1;
}

int __gtsf2(unsigned int a, unsigned int b) {
    int c = __ltsf2(a, b);
    return (c > 0) ? 1 : 0;
}

int __gesf2(unsigned int a, unsigned int b) {
    int c = __ltsf2(a, b);
    return (c >= 0) ? 0 : -1;
}

unsigned int __floatsisf(int i) {
    int sign = 0, exp = 0;
    unsigned long val;
    if (i == 0) return 0;
    if (i < 0) {
        sign = 1;
        val = (unsigned long)(-(long)i);
    } else {
        val = (unsigned long)i;
    }
    exp = F32_BIAS + 23;
    while ((val & 0x00800000UL) == 0 && (val < 0x00800000UL)) {
        val <<= 1;
        exp--;
    }
    while (val & 0xFF000000UL) {
        val >>= 1;
        exp++;
    }
    return f32_pack(sign, exp, val);
}

unsigned int __floatdisf(long l) {
    return __floatsisf((int)l);
}

unsigned int __floatunsisf(unsigned int u) {
    int exp = F32_BIAS + 23;
    unsigned long val = u;
    if (u == 0) return 0;
    while ((val & 0x00800000UL) == 0 && (val < 0x00800000UL)) {
        val <<= 1;
        exp--;
    }
    while (val & 0xFF000000UL) {
        val >>= 1;
        exp++;
    }
    return f32_pack(0, exp, val);
}

unsigned int __floatundisf(unsigned long u) {
    return __floatunsisf((unsigned int)u);
}

int __fixsfsi(unsigned int a) {
    int sign, exp;
    unsigned long mant;
    long val;
    f32_unpack(a, &sign, &exp, &mant);
    if (exp < F32_BIAS) return 0;
    exp -= (F32_BIAS + 23);
    if (exp > 0) mant <<= exp;
    else if (exp < 0) mant >>= (-exp);
    val = (long)mant;
    return (int)(sign ? -val : val);
}

long __fixsfdi(unsigned int a) {
    return (long)__fixsfsi(a);
}

unsigned int __fixunssfsi(unsigned int a) {
    int sign, exp;
    unsigned long mant;
    f32_unpack(a, &sign, &exp, &mant);
    if (sign || exp < F32_BIAS) return 0;
    exp -= (F32_BIAS + 23);
    if (exp > 0) mant <<= exp;
    else if (exp < 0) mant >>= (-exp);
    return (unsigned int)mant;
}

unsigned long __fixunssfdi(unsigned int a) {
    return (unsigned long)__fixunssfsi(a);
}

/* ========================================================================= */
/* Float64 (Double Precision IEEE 754) Core Implementation                   */
/* ========================================================================= */

#define F64_BIAS 1023

static void f64_unpack(const soft_f64 *u, int *sign, int *exp, soft_f64 *mant) {
    *sign = (u->hi >> 31) & 1;
    *exp = (int)((u->hi >> 20) & 0x7FF);
    mant->lo = u->lo;
    mant->hi = u->hi & 0x000FFFFFU;
    if (*exp == 0) {
        if (mant->lo != 0 || mant->hi != 0) *exp = 1;
    } else if (*exp < 2047) {
        mant->hi |= 0x00100000U; /* implicit 1 at bit 52 */
    }
}

static void f64_pack(soft_f64 *res, int sign, int exp, const soft_f64 *mant) {
    if (exp <= 0) {
        res->lo = 0;
        res->hi = (unsigned int)(sign ? 0x80000000U : 0);
        return;
    }
    if (exp >= 2047) {
        res->lo = 0;
        res->hi = (unsigned int)(((unsigned int)sign << 31) | 0x7FF00000U);
        return;
    }
    res->lo = mant->lo;
    res->hi = (mant->hi & 0x000FFFFFU) | (((unsigned int)exp & 0x7FFU) << 20) | ((unsigned int)sign << 31);
}

static void f64_shr(soft_f64 *res, const soft_f64 *a, int count) {
    soft_f64 tmp;
    if (count <= 0) { *res = *a; return; }
    if (count >= 64) { res->lo = 0; res->hi = 0; return; }
    if (count >= 32) {
        tmp.lo = a->hi >> (count - 32);
        tmp.hi = 0;
    } else {
        tmp.lo = (a->lo >> count) | (a->hi << (32 - count));
        tmp.hi = a->hi >> count;
    }
    *res = tmp;
}

static void f64_shl(soft_f64 *res, const soft_f64 *a, int count) {
    soft_f64 tmp;
    if (count <= 0) { *res = *a; return; }
    if (count >= 64) { res->lo = 0; res->hi = 0; return; }
    if (count >= 32) {
        tmp.hi = a->lo << (count - 32);
        tmp.lo = 0;
    } else {
        tmp.hi = (a->hi << count) | (a->lo >> (32 - count));
        tmp.lo = a->lo << count;
    }
    *res = tmp;
}

static int f64_cmp_mant(const soft_f64 *a, const soft_f64 *b) {
    if (a->hi < b->hi) return -1;
    if (a->hi > b->hi) return 1;
    if (a->lo < b->lo) return -1;
    if (a->lo > b->lo) return 1;
    return 0;
}

static void f64_add_mant(soft_f64 *res, const soft_f64 *a, const soft_f64 *b) {
    unsigned int sum_lo = a->lo + b->lo;
    unsigned int carry = (sum_lo < a->lo) ? 1 : 0;
    unsigned int sum_hi = a->hi + b->hi + carry;
    res->lo = sum_lo;
    res->hi = sum_hi;
}

static void f64_sub_mant(soft_f64 *res, const soft_f64 *a, const soft_f64 *b) {
    unsigned int diff_lo = a->lo - b->lo;
    unsigned int borrow = (a->lo < b->lo) ? 1 : 0;
    unsigned int diff_hi = a->hi - b->hi - borrow;
    res->lo = diff_lo;
    res->hi = diff_hi;
}

static void f64_add_impl(soft_f64 *u_res, const soft_f64 *u_a, const soft_f64 *u_b) {
    int sa, sb, ea, eb, er;
    soft_f64 ma, mb, mr;

    f64_unpack(u_a, &sa, &ea, &ma);
    f64_unpack(u_b, &sb, &eb, &mb);

    if ((ma.lo | ma.hi) == 0) { *u_res = *u_b; }
    else if ((mb.lo | mb.hi) == 0) { *u_res = *u_a; }
    else {
        if (ea > eb) {
            f64_shr(&mb, &mb, ea - eb);
            er = ea;
        } else {
            f64_shr(&ma, &ma, eb - ea);
            er = eb;
        }

        if (sa == sb) {
            f64_add_mant(&mr, &ma, &mb);
            if (mr.hi & 0x00200000U) {
                f64_shr(&mr, &mr, 1);
                er++;
            }
            f64_pack(u_res, sa, er, &mr);
        } else {
            int sr;
            if (f64_cmp_mant(&ma, &mb) >= 0) {
                f64_sub_mant(&mr, &ma, &mb);
                sr = sa;
            } else {
                f64_sub_mant(&mr, &mb, &ma);
                sr = sb;
            }
            if ((mr.lo | mr.hi) == 0) {
                u_res->lo = 0;
                u_res->hi = 0;
            } else {
                while ((mr.hi & 0x00100000U) == 0 && er > 0) {
                    f64_shl(&mr, &mr, 1);
                    er--;
                }
                f64_pack(u_res, sr, er, &mr);
            }
        }
    }
}

static void f64_sub_impl(soft_f64 *u_res, const soft_f64 *u_a, const soft_f64 *u_b) {
    soft_f64 neg_b = *u_b;
    neg_b.hi ^= 0x80000000U;
    f64_add_impl(u_res, u_a, &neg_b);
}

static void f64_mul_impl(soft_f64 *u_res, const soft_f64 *u_a, const soft_f64 *u_b) {
    int sa, sb, ea, eb, er;
    soft_f64 ma, mb;
    soft_f128 a128, b128, p128;

    f64_unpack(u_a, &sa, &ea, &ma);
    f64_unpack(u_b, &sb, &eb, &mb);

    if ((ma.lo | ma.hi) == 0 || (mb.lo | mb.hi) == 0) {
        u_res->lo = 0;
        u_res->hi = (sa ^ sb) ? 0x80000000U : 0;
    } else {
        unsigned int p00_hi, p00_lo, p01_hi, p01_lo, p10_hi, p10_lo, p11_hi, p11_lo;
        soft_f128 t1, t2;
        soft_f64 mr;

        er = ea + eb - F64_BIAS;
        u128_zero(&a128); u128_zero(&b128);
        a128.w[0] = ma.lo; a128.w[1] = ma.hi;
        b128.w[0] = mb.lo; b128.w[1] = mb.hi;

        mul32x32_to_64(&p00_hi, &p00_lo, a128.w[0], b128.w[0]);
        mul32x32_to_64(&p01_hi, &p01_lo, a128.w[0], b128.w[1]);
        mul32x32_to_64(&p10_hi, &p10_lo, a128.w[1], b128.w[0]);
        mul32x32_to_64(&p11_hi, &p11_lo, a128.w[1], b128.w[1]);

        u128_zero(&p128);
        p128.w[0] = p00_lo;
        p128.w[1] = p00_hi;

        u128_zero(&t1); t1.w[1] = p01_lo; t1.w[2] = p01_hi;
        u128_add(&p128, &p128, &t1);

        u128_zero(&t2); t2.w[1] = p10_lo; t2.w[2] = p10_hi;
        u128_add(&p128, &p128, &t2);

        u128_zero(&t1); t1.w[2] = p11_lo; t1.w[3] = p11_hi;
        u128_add(&p128, &p128, &t1);

        u128_shr(&p128, &p128, 52);
        mr.lo = p128.w[0];
        mr.hi = p128.w[1];

        if (mr.hi & 0x00200000U) {
            f64_shr(&mr, &mr, 1);
            er++;
        }
        while ((mr.hi & 0x00100000U) == 0 && er > 0) {
            f64_shl(&mr, &mr, 1);
            er--;
        }
        f64_pack(u_res, sa ^ sb, er, &mr);
    }
}

static void f64_div_impl(soft_f64 *u_res, const soft_f64 *u_a, const soft_f64 *u_b) {
    int sa, sb, ea, eb, er;
    soft_f64 ma, mb;
    soft_f128 num, den, quot;

    f64_unpack(u_a, &sa, &ea, &ma);
    f64_unpack(u_b, &sb, &eb, &mb);

    if ((mb.lo | mb.hi) == 0) {
        u_res->lo = 0;
        u_res->hi = ((unsigned int)(sa ^ sb) << 31) | 0x7FF00000U;
    } else if ((ma.lo | ma.hi) == 0) {
        u_res->lo = 0;
        u_res->hi = (sa ^ sb) ? 0x80000000U : 0;
    } else {
        soft_f64 mr;
        er = ea - eb + F64_BIAS;
        u128_zero(&num);
        u128_zero(&den);
        num.w[0] = ma.lo; num.w[1] = ma.hi;
        u128_shl(&num, &num, 52);

        den.w[0] = mb.lo; den.w[1] = mb.hi;
        u128_div(&quot, NULL, &num, &den);

        mr.lo = quot.w[0];
        mr.hi = quot.w[1];
        while ((mr.lo | mr.hi) && (mr.hi & 0x00100000U) == 0 && er > 0) {
            f64_shl(&mr, &mr, 1);
            er--;
        }
        if (mr.hi & 0x00200000U) {
            f64_shr(&mr, &mr, 1);
            er++;
        }
        f64_pack(u_res, sa ^ sb, er, &mr);
    }
}

static int f64_eq_impl(const soft_f64 *a, const soft_f64 *b) {
    if (((a->hi & 0x7FFFFFFFU) | a->lo) == 0 && ((b->hi & 0x7FFFFFFFU) | b->lo) == 0) return 0;
    return (a->hi == b->hi && a->lo == b->lo) ? 0 : 1;
}

static int f64_lt_impl(const soft_f64 *a, const soft_f64 *b) {
    int sa = (a->hi >> 31) & 1;
    int sb = (b->hi >> 31) & 1;
    if (((a->hi & 0x7FFFFFFFU) | a->lo) == 0 && ((b->hi & 0x7FFFFFFFU) | b->lo) == 0) return 0;
    if (sa != sb) return sa ? -1 : 1;
    if (sa) {
        if (a->hi > b->hi) return -1;
        if (a->hi < b->hi) return 1;
        return (a->lo > b->lo) ? -1 : (a->lo < b->lo ? 1 : 0);
    }
    if (a->hi < b->hi) return -1;
    if (a->hi > b->hi) return 1;
    return (a->lo < b->lo) ? -1 : (a->lo > b->lo ? 1 : 0);
}

static void f64_from_int_impl(soft_f64 *res, int i) {
    int sign = 0, exp = 0;
    soft_f64 val;
    val.lo = 0; val.hi = 0;
    if (i == 0) { res->lo = 0; res->hi = 0; }
    else {
        if (i < 0) {
            sign = 1;
            val.lo = (unsigned int)(-(long)i);
        } else {
            val.lo = (unsigned int)i;
        }
        exp = F64_BIAS + 52;
        while ((val.hi & 0x00100000U) == 0) {
            f64_shl(&val, &val, 1);
            exp--;
        }
        f64_pack(res, sign, exp, &val);
    }
}

static void f64_from_uint_impl(soft_f64 *res, unsigned int u) {
    int exp = 0;
    soft_f64 val;
    val.lo = 0; val.hi = 0;
    if (u == 0) { res->lo = 0; res->hi = 0; }
    else {
        val.lo = u;
        exp = F64_BIAS + 52;
        while ((val.hi & 0x00100000U) == 0) {
            f64_shl(&val, &val, 1);
            exp--;
        }
        f64_pack(res, 0, exp, &val);
    }
}

static int f64_to_int_impl(const soft_f64 *a) {
    soft_f64 mant;
    int sign, exp;
    long val;
    f64_unpack(a, &sign, &exp, &mant);
    if (exp < F64_BIAS) return 0;
    exp -= (F64_BIAS + 52);
    if (exp > 0) f64_shl(&mant, &mant, exp);
    else if (exp < 0) f64_shr(&mant, &mant, -exp);
    val = (long)mant.lo;
    return (int)(sign ? -val : val);
}

static unsigned int f64_to_uint_impl(const soft_f64 *a) {
    soft_f64 mant;
    int sign, exp;
    f64_unpack(a, &sign, &exp, &mant);
    if (sign || exp < F64_BIAS) return 0;
    exp -= (F64_BIAS + 52);
    if (exp > 0) f64_shl(&mant, &mant, exp);
    else if (exp < 0) f64_shr(&mant, &mant, -exp);
    return mant.lo;
}

static void f64_from_f32_impl(soft_f64 *res, unsigned int a) {
    int sign, exp;
    unsigned long mant;
    soft_f64 m64;
    f32_unpack(a, &sign, &exp, &mant);
    if (mant == 0 && exp == 0) {
        res->lo = 0;
        res->hi = sign ? 0x80000000U : 0;
    } else {
        exp = exp - F32_BIAS + F64_BIAS;
        m64.lo = (unsigned int)((mant & F32_MANT_MASK) << (52 - 23 - 32 + 32));
        m64.hi = (unsigned int)((mant & F32_MANT_MASK) >> (23 - 20));
        f64_pack(res, sign, exp, &m64);
    }
}

static unsigned int f64_to_f32_impl(const soft_f64 *a) {
    soft_f64 mant;
    int sign, exp;
    unsigned long m32;
    f64_unpack(a, &sign, &exp, &mant);
    if ((mant.lo | mant.hi) == 0 && exp == 0) return (unsigned int)(sign ? F32_SIGN_MASK : 0);
    exp = exp - F64_BIAS + F32_BIAS;
    m32 = ((mant.hi & 0x000FFFFFU) << 3) | (mant.lo >> 29);
    return f32_pack(sign, exp, m32);
}

static void parse_dec_f64(const char *str, soft_f64 *out) {
    const char *p = str;
    int sign = 0;
    soft_f64 val, ten, digit;
    int exp_sign = 0, exp_val = 0;

    f64_from_int_impl(&val, 0);
    f64_from_int_impl(&ten, 10);

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '-') { sign = 1; p++; }
    else if (*p == '+') { p++; }

    while (*p >= '0' && *p <= '9') {
        f64_mul_impl(&val, &val, &ten);
        f64_from_int_impl(&digit, *p - '0');
        f64_add_impl(&val, &val, &digit);
        p++;
    }

    if (*p == '.') {
        soft_f64 frac_div;
        f64_from_int_impl(&frac_div, 1);
        p++;
        while (*p >= '0' && *p <= '9') {
            f64_mul_impl(&frac_div, &frac_div, &ten);
            f64_from_int_impl(&digit, *p - '0');
            f64_div_impl(&digit, &digit, &frac_div);
            f64_add_impl(&val, &val, &digit);
            p++;
        }
    }

    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '-') { exp_sign = 1; p++; }
        else if (*p == '+') { p++; }
        while (*p >= '0' && *p <= '9') {
            exp_val = exp_val * 10 + (*p - '0');
            p++;
        }
        while (exp_val > 0) {
            if (exp_sign) f64_div_impl(&val, &val, &ten);
            else f64_mul_impl(&val, &val, &ten);
            exp_val--;
        }
    }

    if (sign) val.hi ^= 0x80000000U;
    *out = val;
}

/* ========================================================================= */
/* Float64 Target Wrappers                                                   */
/* ========================================================================= */

#if defined(TARGET_RISCV32) || defined(__riscv)

double __adddf3(double a, double b) {
    f64_cast ca, cb, cr;
    soft_f64 sa, sb, sr;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    f64_add_impl(&sr, &sa, &sb);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __subdf3(double a, double b) {
    f64_cast ca, cb, cr;
    soft_f64 sa, sb, sr;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    f64_sub_impl(&sr, &sa, &sb);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __muldf3(double a, double b) {
    f64_cast ca, cb, cr;
    soft_f64 sa, sb, sr;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    f64_mul_impl(&sr, &sa, &sb);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __divdf3(double a, double b) {
    f64_cast ca, cb, cr;
    soft_f64 sa, sb, sr;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    f64_div_impl(&sr, &sa, &sb);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __negdf2(double a) {
    f64_cast ca;
    ca.d = a;
    ca.u.hi ^= 0x80000000U;
    return ca.d;
}

int __eqdf2(double a, double b) {
    f64_cast ca, cb;
    soft_f64 sa, sb;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    return f64_eq_impl(&sa, &sb);
}

int __nedf2(double a, double b) {
    return __eqdf2(a, b);
}

int __ltdf2(double a, double b) {
    f64_cast ca, cb;
    soft_f64 sa, sb;
    ca.d = a; cb.d = b;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    sb.lo = cb.u.lo; sb.hi = cb.u.hi;
    return f64_lt_impl(&sa, &sb);
}

int __ledf2(double a, double b) {
    int c = __ltdf2(a, b);
    return (c <= 0) ? 0 : 1;
}

int __gtdf2(double a, double b) {
    int c = __ltdf2(a, b);
    return (c > 0) ? 1 : 0;
}

int __gedf2(double a, double b) {
    int c = __ltdf2(a, b);
    return (c >= 0) ? 0 : -1;
}

double __floatsidf(int i) {
    soft_f64 sr;
    f64_cast cr;
    f64_from_int_impl(&sr, i);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __floatdidf(long l) {
    return __floatsidf((int)l);
}

double __floatunsidf(unsigned int u) {
    soft_f64 sr;
    f64_cast cr;
    f64_from_uint_impl(&sr, u);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

double __floatundidf(unsigned long u) {
    return __floatunsidf((unsigned int)u);
}

int __fixdfsi(double a) {
    f64_cast ca;
    soft_f64 sa;
    ca.d = a;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    return f64_to_int_impl(&sa);
}

long __fixdfdi(double a) {
    return (long)__fixdfsi(a);
}

unsigned int __fixunsdfsi(double a) {
    f64_cast ca;
    soft_f64 sa;
    ca.d = a;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    return f64_to_uint_impl(&sa);
}

unsigned long __fixunsdfdi(double a) {
    return (unsigned long)__fixunsdfsi(a);
}

double __extendsfdf2(float a) {
    soft_f64 sr;
    f32_cast ca;
    f64_cast cr;
    ca.f = a;
    f64_from_f32_impl(&sr, ca.u);
    cr.u.lo = sr.lo; cr.u.hi = sr.hi;
    return cr.d;
}

float __truncdfsf2(double a) {
    f64_cast ca;
    soft_f64 sa;
    f32_cast cr;
    ca.d = a;
    sa.lo = ca.u.lo; sa.hi = ca.u.hi;
    cr.u = f64_to_f32_impl(&sa);
    return cr.f;
}

int soft_strto_f64(const char *str, void *out) {
    parse_dec_f64(str, (soft_f64 *)out);
    return 0;
}

#elif defined(TARGET_I386) || defined(__i386__) || defined(TARGET_32BIT)

void *__adddf3(void *res, const void *a, const void *b) {
    f64_add_impl((soft_f64 *)res, (const soft_f64 *)a, (const soft_f64 *)b);
    return res;
}

void *__subdf3(void *res, const void *a, const void *b) {
    f64_sub_impl((soft_f64 *)res, (const soft_f64 *)a, (const soft_f64 *)b);
    return res;
}

void *__muldf3(void *res, const void *a, const void *b) {
    f64_mul_impl((soft_f64 *)res, (const soft_f64 *)a, (const soft_f64 *)b);
    return res;
}

void *__divdf3(void *res, const void *a, const void *b) {
    f64_div_impl((soft_f64 *)res, (const soft_f64 *)a, (const soft_f64 *)b);
    return res;
}

void *__negdf2(void *res, const void *a) {
    soft_f64 *r = (soft_f64 *)res;
    *r = *(const soft_f64 *)a;
    r->hi ^= 0x80000000U;
    return res;
}

int __eqdf2(const void *a, const void *b) {
    return f64_eq_impl((const soft_f64 *)a, (const soft_f64 *)b);
}

int __nedf2(const void *a, const void *b) {
    return __eqdf2(a, b);
}

int __ltdf2(const void *a, const void *b) {
    return f64_lt_impl((const soft_f64 *)a, (const soft_f64 *)b);
}

int __ledf2(const void *a, const void *b) {
    int c = __ltdf2(a, b);
    return (c <= 0) ? 0 : 1;
}

int __gtdf2(const void *a, const void *b) {
    int c = __ltdf2(a, b);
    return (c > 0) ? 1 : 0;
}

int __gedf2(const void *a, const void *b) {
    int c = __ltdf2(a, b);
    return (c >= 0) ? 0 : -1;
}

void *__floatsidf(void *res, int i) {
    f64_from_int_impl((soft_f64 *)res, i);
    return res;
}

void *__floatdidf(void *res, long l) {
    return __floatsidf(res, (int)l);
}

void *__floatunsidf(void *res, unsigned int u) {
    f64_from_uint_impl((soft_f64 *)res, u);
    return res;
}

void *__floatundidf(void *res, unsigned long u) {
    return __floatunsidf(res, (unsigned int)u);
}

int __fixdfsi(const void *a) {
    return f64_to_int_impl((const soft_f64 *)a);
}

long __fixdfdi(const void *a) {
    return (long)__fixdfsi(a);
}

unsigned int __fixunsdfsi(const void *a) {
    return f64_to_uint_impl((const soft_f64 *)a);
}

unsigned long __fixunsdfdi(const void *a) {
    return (unsigned long)__fixunsdfsi(a);
}

void *__extendsfdf2(void *res, unsigned int a) {
    f64_from_f32_impl((soft_f64 *)res, a);
    return res;
}

unsigned int __truncdfsf2(const void *a) {
    return f64_to_f32_impl((const soft_f64 *)a);
}

int soft_strto_f64(const char *str, void *out) {
    parse_dec_f64(str, (soft_f64 *)out);
    return 0;
}

#else

unsigned long __adddf3(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb, cr;
    soft_f64 u_a, u_b, u_res;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    f64_add_impl(&u_res, &u_a, &u_b);
    cr.u.lo = u_res.lo; cr.u.hi = u_res.hi;
    return f64_to_ulong(&cr);
}

unsigned long __subdf3(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb, cr;
    soft_f64 u_a, u_b, u_res;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    f64_sub_impl(&u_res, &u_a, &u_b);
    cr.u.lo = u_res.lo; cr.u.hi = u_res.hi;
    return f64_to_ulong(&cr);
}

unsigned long __muldf3(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb, cr;
    soft_f64 u_a, u_b, u_res;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    f64_mul_impl(&u_res, &u_a, &u_b);
    cr.u.lo = u_res.lo; cr.u.hi = u_res.hi;
    return f64_to_ulong(&cr);
}

unsigned long __divdf3(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb, cr;
    soft_f64 u_a, u_b, u_res;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    f64_div_impl(&u_res, &u_a, &u_b);
    cr.u.lo = u_res.lo; cr.u.hi = u_res.hi;
    return f64_to_ulong(&cr);
}

unsigned long __negdf2(unsigned long a_val) {
    f64_cast c;
    f64_from_ulong(&c, a_val);
    c.u.hi ^= 0x80000000U;
    return f64_to_ulong(&c);
}

int __eqdf2(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb;
    soft_f64 u_a, u_b;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    return f64_eq_impl(&u_a, &u_b);
}

int __nedf2(unsigned long a, unsigned long b) {
    return __eqdf2(a, b);
}

int __ltdf2(unsigned long a_val, unsigned long b_val) {
    f64_cast ca, cb;
    soft_f64 u_a, u_b;
    f64_from_ulong(&ca, a_val);
    f64_from_ulong(&cb, b_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    u_b.lo = cb.u.lo; u_b.hi = cb.u.hi;
    return f64_lt_impl(&u_a, &u_b);
}

int __ledf2(unsigned long a, unsigned long b) {
    int c = __ltdf2(a, b);
    return (c <= 0) ? 0 : 1;
}

int __gtdf2(unsigned long a, unsigned long b) {
    int c = __ltdf2(a, b);
    return (c > 0) ? 1 : 0;
}

int __gedf2(unsigned long a, unsigned long b) {
    int c = __ltdf2(a, b);
    return (c >= 0) ? 0 : -1;
}

unsigned long __floatsidf(int i) {
    return __floatdidf((long)i);
}

unsigned long __floatdidf(long l) {
    f64_cast cr;
    soft_f64 res;
    f64_from_int_impl(&res, (int)l);
    cr.u.lo = res.lo; cr.u.hi = res.hi;
    return f64_to_ulong(&cr);
}

unsigned long __floatunsidf(unsigned int u) {
    return __floatundidf((unsigned long)u);
}

unsigned long __floatundidf(unsigned long u) {
    f64_cast cr;
    soft_f64 res;
    f64_from_uint_impl(&res, (unsigned int)u);
    cr.u.lo = res.lo; cr.u.hi = res.hi;
    return f64_to_ulong(&cr);
}

int __fixdfsi(unsigned long a) {
    return (int)__fixdfdi(a);
}

long __fixdfdi(unsigned long a_val) {
    f64_cast ca;
    soft_f64 u_a;
    f64_from_ulong(&ca, a_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    return (long)f64_to_int_impl(&u_a);
}

unsigned int __fixunsdfsi(unsigned long a) {
    return (unsigned int)__fixunsdfdi(a);
}

unsigned long __fixunsdfdi(unsigned long a_val) {
    f64_cast ca;
    soft_f64 u_a;
    f64_from_ulong(&ca, a_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    return (unsigned long)f64_to_uint_impl(&u_a);
}

unsigned long __extendsfdf2(unsigned int a) {
    f64_cast cr;
    soft_f64 res;
    f64_from_f32_impl(&res, a);
    cr.u.lo = res.lo; cr.u.hi = res.hi;
    return f64_to_ulong(&cr);
}

unsigned int __truncdfsf2(unsigned long a_val) {
    f64_cast ca;
    soft_f64 u_a;
    f64_from_ulong(&ca, a_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    return f64_to_f32_impl(&u_a);
}

int soft_strto_f64(const char *str, void *out) {
    soft_f64 val;
    parse_dec_f64(str, &val);
    *(unsigned long *)out = ((unsigned long)val.hi << 32) | (unsigned long)val.lo;
    return 0;
}

#endif

/* ========================================================================= */
/* Float128 (128-bit Quad Precision IEEE 754) Implementation                 */
/* ========================================================================= */

#define F128_BIAS 16383

static void f128_unpack(const soft_f128 *u, int *sign, int *exp, soft_f128 *mant) {
    *sign = (u->w[3] >> 31) & 1;
    *exp = (int)((u->w[3] >> 16) & 0x7FFF);
    mant->w[0] = u->w[0];
    mant->w[1] = u->w[1];
    mant->w[2] = u->w[2];
    mant->w[3] = u->w[3] & 0xFFFFU;
    if (*exp == 0) {
        if (!u128_is_zero(mant)) *exp = 1;
    } else if (*exp < 0x7FFF) {
        mant->w[3] |= 0x10000U; /* implicit 1 at bit 112 */
    }
}

static void f128_pack(soft_f128 *res, int sign, int exp, const soft_f128 *mant) {
    if (exp <= 0) {
        u128_zero(res);
        if (sign) res->w[3] |= 0x80000000U;
        return;
    }
    if (exp >= 0x7FFF) {
        u128_zero(res);
        res->w[3] = (unsigned int)(((unsigned int)sign << 31) | 0x7FFF0000U);
        return;
    }
    res->w[0] = mant->w[0];
    res->w[1] = mant->w[1];
    res->w[2] = mant->w[2];
    res->w[3] = (mant->w[3] & 0xFFFFU) | (((unsigned int)exp & 0x7FFFU) << 16) | ((unsigned int)sign << 31);
}

void *__addtf3(void *res, const void *a_ptr, const void *b_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    const soft_f128 *b = (const soft_f128 *)b_ptr;
    soft_f128 *r = (soft_f128 *)res;
    int sa, sb, ea, eb, er;
    soft_f128 ma, mb, mr;

    f128_unpack(a, &sa, &ea, &ma);
    f128_unpack(b, &sb, &eb, &mb);

    if (u128_is_zero(&ma)) { *r = *b; return r; }
    if (u128_is_zero(&mb)) { *r = *a; return r; }

    if (ea > eb) {
        u128_shr(&mb, &mb, ea - eb);
        er = ea;
    } else {
        u128_shr(&ma, &ma, eb - ea);
        er = eb;
    }

    if (sa == sb) {
        u128_add(&mr, &ma, &mb);
        if (mr.w[3] & 0x20000U) {
            u128_shr(&mr, &mr, 1);
            er++;
        }
        f128_pack(r, sa, er, &mr);
    } else {
        int sr;
        if (u128_cmp(&ma, &mb) >= 0) {
            u128_sub(&mr, &ma, &mb);
            sr = sa;
        } else {
            u128_sub(&mr, &mb, &ma);
            sr = sb;
        }
        if (u128_is_zero(&mr)) {
            u128_zero(r);
            return r;
        }
        while ((mr.w[3] & 0x10000U) == 0 && er > 0) {
            u128_shl(&mr, &mr, 1);
            er--;
        }
        f128_pack(r, sr, er, &mr);
    }
    return r;
}

void *__subtf3(void *res, const void *a_ptr, const void *b_ptr) {
    soft_f128 neg_b = *(const soft_f128 *)b_ptr;
    neg_b.w[3] ^= 0x80000000U;
    return __addtf3(res, a_ptr, &neg_b);
}

void *__multf3(void *res, const void *a_ptr, const void *b_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    const soft_f128 *b = (const soft_f128 *)b_ptr;
    soft_f128 *r = (soft_f128 *)res;
    int sa, sb, ea, eb, er, i, j;
    soft_f128 ma, mb, mr;
    unsigned int prod[8];

    f128_unpack(a, &sa, &ea, &ma);
    f128_unpack(b, &sb, &eb, &mb);

    if (u128_is_zero(&ma) || u128_is_zero(&mb)) {
        u128_zero(r);
        if (sa ^ sb) r->w[3] |= 0x80000000U;
        return r;
    }

    er = ea + eb - F128_BIAS;
    for (i = 0; i < 8; i++) prod[i] = 0;

    for (i = 0; i < 4; i++) {
        unsigned int carry = 0;
        for (j = 0; j < 4; j++) {
            unsigned int p_hi, p_lo;
            unsigned int prev = prod[i + j];
            unsigned int sum1, sum2;
            unsigned int c1 = 0, c2 = 0;
            mul32x32_to_64(&p_hi, &p_lo, ma.w[i], mb.w[j]);
            sum1 = prev + p_lo;
            if (sum1 < prev) c1 = 1;
            sum2 = sum1 + carry;
            if (sum2 < sum1) c2 = 1;
            prod[i + j] = sum2;
            carry = p_hi + c1 + c2;
        }
        {
            int k = i + 4;
            while (carry && k < 8) {
                unsigned int prev = prod[k];
                prod[k] += carry;
                carry = (prod[k] < prev) ? 1 : 0;
                k++;
            }
        }
    }

    /* Shift 256-bit product right by 112 bits */
    {
        mr.w[0] = (prod[3] >> 16) | (prod[4] << 16);
        mr.w[1] = (prod[4] >> 16) | (prod[5] << 16);
        mr.w[2] = (prod[5] >> 16) | (prod[6] << 16);
        mr.w[3] = (prod[6] >> 16) | (prod[7] << 16);
    }

    if (mr.w[3] & 0x20000U) {
        u128_shr(&mr, &mr, 1);
        er++;
    }
    while ((mr.w[3] & 0x10000U) == 0 && er > 0) {
        u128_shl(&mr, &mr, 1);
        er--;
    }
    f128_pack(r, sa ^ sb, er, &mr);
    return r;
}

void *__divtf3(void *res, const void *a_ptr, const void *b_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    const soft_f128 *b = (const soft_f128 *)b_ptr;
    soft_f128 *r = (soft_f128 *)res;
    int sa, sb, ea, eb, er, i;
    soft_f128 ma, mb, quot, rem;

    f128_unpack(a, &sa, &ea, &ma);
    f128_unpack(b, &sb, &eb, &mb);

    if (u128_is_zero(&mb)) {
        u128_zero(r);
        r->w[3] = (unsigned int)(((unsigned int)(sa ^ sb) << 31) | 0x7FFF0000U);
        return r;
    }
    if (u128_is_zero(&ma)) {
        u128_zero(r);
        if (sa ^ sb) r->w[3] |= 0x80000000U;
        return r;
    }

    er = ea - eb + F128_BIAS;
    u128_zero(&quot);
    u128_zero(&rem);

    /* Divide (ma << 112) by mb using bit-by-bit division */
    for (i = 224; i >= 0; i--) {
        int bit = 0;
        if (i >= 112) {
            int src_bit = i - 112;
            if (src_bit < 128) {
                bit = (ma.w[src_bit / 32] >> (src_bit % 32)) & 1;
            }
        }
        u128_shl(&rem, &rem, 1);
        rem.w[0] |= (unsigned int)bit;
        if (u128_cmp(&rem, &mb) >= 0) {
            u128_sub(&rem, &rem, &mb);
            if (i < 128) {
                quot.w[i / 32] |= (1U << (i % 32));
            }
        }
    }

    while (!u128_is_zero(&quot) && (quot.w[3] & 0x10000U) == 0 && er > 0) {
        u128_shl(&quot, &quot, 1);
        er--;
    }
    if (quot.w[3] & 0x20000U) {
        u128_shr(&quot, &quot, 1);
        er++;
    }
    f128_pack(r, sa ^ sb, er, &quot);
    return r;
}

void *__negtf2(void *res, const void *a_ptr) {
    soft_f128 *r = (soft_f128 *)res;
    *r = *(const soft_f128 *)a_ptr;
    r->w[3] ^= 0x80000000U;
    return r;
}

int __eqtf2(const void *a_ptr, const void *b_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    const soft_f128 *b = (const soft_f128 *)b_ptr;
    if (u128_is_zero(a) && u128_is_zero(b)) return 0;
    return (u128_cmp(a, b) == 0) ? 0 : 1;
}

int __netf2(const void *a, const void *b) {
    return __eqtf2(a, b);
}

int __lttf2(const void *a_ptr, const void *b_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    const soft_f128 *b = (const soft_f128 *)b_ptr;
    int sa = (a->w[3] >> 31) & 1;
    int sb = (b->w[3] >> 31) & 1;
    if (u128_is_zero(a) && u128_is_zero(b)) return 0;
    if (sa != sb) return sa ? -1 : 1;
    if (sa) return (u128_cmp(a, b) > 0) ? -1 : (u128_cmp(a, b) < 0 ? 1 : 0);
    return (u128_cmp(a, b) < 0) ? -1 : (u128_cmp(a, b) > 0 ? 1 : 0);
}

int __letf2(const void *a, const void *b) {
    int c = __lttf2(a, b);
    return (c <= 0) ? 0 : 1;
}

int __gttf2(const void *a, const void *b) {
    int c = __lttf2(a, b);
    return (c > 0) ? 1 : 0;
}

int __getf2(const void *a, const void *b) {
    int c = __lttf2(a, b);
    return (c >= 0) ? 0 : -1;
}

void *__floatsitf(void *res, int i) {
    return __floatditf(res, (long)i);
}

void *__floatditf(void *res, long l) {
    soft_f128 *r = (soft_f128 *)res;
    int sign = 0, exp = 0;
    soft_f128 val;
    u128_zero(&val);
    if (l == 0) { u128_zero(r); return r; }
    if (l < 0) {
        sign = 1;
        val.w[0] = (unsigned int)(-(long)l);
    } else {
        val.w[0] = (unsigned int)l;
    }
    exp = F128_BIAS + 112;
    while ((val.w[3] & 0x10000U) == 0) {
        u128_shl(&val, &val, 1);
        exp--;
    }
    f128_pack(r, sign, exp, &val);
    return r;
}

void *__floatunsitf(void *res, unsigned int u) {
    return __floatunditf(res, (unsigned long)u);
}

void *__floatunditf(void *res, unsigned long u) {
    soft_f128 *r = (soft_f128 *)res;
    int exp = 0;
    soft_f128 val;
    u128_zero(&val);
    if (u == 0) { u128_zero(r); return r; }
    val.w[0] = (unsigned int)u;
    exp = F128_BIAS + 112;
    while ((val.w[3] & 0x10000U) == 0) {
        u128_shl(&val, &val, 1);
        exp--;
    }
    f128_pack(r, 0, exp, &val);
    return r;
}

int __fixtfsi(const void *a) {
    return (int)__fixtfdi(a);
}

long __fixtfdi(const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    int sign, exp;
    soft_f128 mant;
    long val;
    f128_unpack(a, &sign, &exp, &mant);
    if (exp < F128_BIAS) return 0;
    exp -= (F128_BIAS + 112);
    if (exp > 0) u128_shl(&mant, &mant, exp);
    else if (exp < 0) u128_shr(&mant, &mant, -exp);
    val = (long)mant.w[0];
    return sign ? -val : val;
}

unsigned int __fixunstfsi(const void *a) {
    return (unsigned int)__fixunstfdi(a);
}

unsigned long __fixunstfdi(const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    int sign, exp;
    soft_f128 mant;
    f128_unpack(a, &sign, &exp, &mant);
    if (sign || exp < F128_BIAS) return 0;
    exp -= (F128_BIAS + 112);
    if (exp > 0) u128_shl(&mant, &mant, exp);
    else if (exp < 0) u128_shr(&mant, &mant, -exp);
    return (unsigned long)mant.w[0];
}

void *__extendsftf2(void *res, unsigned int a) {
    soft_f128 *r = (soft_f128 *)res;
    int sign, exp;
    unsigned long mant;
    soft_f128 m128;
    f32_unpack(a, &sign, &exp, &mant);
    if (mant == 0 && exp == 0) {
        u128_zero(r);
        if (sign) r->w[3] |= 0x80000000U;
        return r;
    }
    exp = exp - F32_BIAS + F128_BIAS;
    u128_zero(&m128);
    m128.w[0] = (unsigned int)mant;
    u128_shl(&m128, &m128, 112 - 23);
    f128_pack(r, sign, exp, &m128);
    return r;
}

#if defined(TARGET_RISCV32) || defined(__riscv)

void *__extenddftf2(void *res, const void *a_ptr) {
    soft_f128 *r = (soft_f128 *)res;
    const soft_f64 *u_a = (const soft_f64 *)a_ptr;
    soft_f64 mant;
    soft_f128 m128;
    int sign, exp;
    f64_unpack(u_a, &sign, &exp, &mant);
    if ((mant.lo | mant.hi) == 0 && exp == 0) {
        u128_zero(r);
        if (sign) r->w[3] |= 0x80000000U;
        return r;
    }
    exp = exp - F64_BIAS + F128_BIAS;
    u128_zero(&m128);
    m128.w[0] = mant.lo;
    m128.w[1] = mant.hi;
    u128_shl(&m128, &m128, 112 - 52);
    f128_pack(r, sign, exp, &m128);
    return r;
}

double __trunctfdf2(const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    soft_f64 r;
    soft_f64 m64;
    soft_f128 mant;
    f64_cast cr;
    int sign, exp;
    f128_unpack(a, &sign, &exp, &mant);
    if (u128_is_zero(&mant) && exp == 0) {
        r.lo = 0;
        r.hi = sign ? 0x80000000U : 0;
    } else {
        exp = exp - F128_BIAS + F64_BIAS;
        u128_shr(&mant, &mant, 112 - 52);
        m64.lo = mant.w[0];
        m64.hi = mant.w[1];
        f64_pack(&r, sign, exp, &m64);
    }
    cr.u.lo = r.lo; cr.u.hi = r.hi;
    return cr.d;
}

#elif defined(TARGET_I386) || defined(__i386__) || defined(TARGET_32BIT)

void *__extenddftf2(void *res, const void *a_ptr) {
    soft_f128 *r = (soft_f128 *)res;
    const soft_f64 *u_a = (const soft_f64 *)a_ptr;
    soft_f64 mant;
    soft_f128 m128;
    int sign, exp;
    f64_unpack(u_a, &sign, &exp, &mant);
    if ((mant.lo | mant.hi) == 0 && exp == 0) {
        u128_zero(r);
        if (sign) r->w[3] |= 0x80000000U;
        return r;
    }
    exp = exp - F64_BIAS + F128_BIAS;
    u128_zero(&m128);
    m128.w[0] = mant.lo;
    m128.w[1] = mant.hi;
    u128_shl(&m128, &m128, 112 - 52);
    f128_pack(r, sign, exp, &m128);
    return r;
}

void *__trunctfdf2(void *res, const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    soft_f64 *r = (soft_f64 *)res;
    soft_f64 m64;
    soft_f128 mant;
    int sign, exp;
    f128_unpack(a, &sign, &exp, &mant);
    if (u128_is_zero(&mant) && exp == 0) {
        r->lo = 0;
        r->hi = sign ? 0x80000000U : 0;
    } else {
        exp = exp - F128_BIAS + F64_BIAS;
        u128_shr(&mant, &mant, 112 - 52);
        m64.lo = mant.w[0];
        m64.hi = mant.w[1];
        f64_pack(r, sign, exp, &m64);
    }
    return res;
}

#else

void *__extenddftf2(void *res, unsigned long a_val) {
    soft_f128 *r = (soft_f128 *)res;
    f64_cast ca;
    soft_f64 u_a, mant;
    soft_f128 m128;
    int sign, exp;
    f64_from_ulong(&ca, a_val);
    u_a.lo = ca.u.lo; u_a.hi = ca.u.hi;
    f64_unpack(&u_a, &sign, &exp, &mant);
    if ((mant.lo | mant.hi) == 0 && exp == 0) {
        u128_zero(r);
        if (sign) r->w[3] |= 0x80000000U;
        return r;
    }
    exp = exp - F64_BIAS + F128_BIAS;
    u128_zero(&m128);
    m128.w[0] = mant.lo;
    m128.w[1] = mant.hi;
    u128_shl(&m128, &m128, 112 - 52);
    f128_pack(r, sign, exp, &m128);
    return r;
}

unsigned long __trunctfdf2(const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    f64_cast cr;
    soft_f64 res, m64;
    soft_f128 mant;
    int sign, exp;
    f128_unpack(a, &sign, &exp, &mant);
    if (u128_is_zero(&mant) && exp == 0) {
        res.lo = 0;
        res.hi = sign ? 0x80000000U : 0;
    } else {
        exp = exp - F128_BIAS + F64_BIAS;
        u128_shr(&mant, &mant, 112 - 52);
        m64.lo = mant.w[0];
        m64.hi = mant.w[1];
        f64_pack(&res, sign, exp, &m64);
    }
    cr.u.lo = res.lo;
    cr.u.hi = res.hi;
    return f64_to_ulong(&cr);
}

#endif

unsigned int __trunctfsf2(const void *a_ptr) {
    const soft_f128 *a = (const soft_f128 *)a_ptr;
    int sign, exp;
    soft_f128 mant;
    f128_unpack(a, &sign, &exp, &mant);
    if (u128_is_zero(&mant) && exp == 0) return (unsigned int)(sign ? F32_SIGN_MASK : 0);
    exp = exp - F128_BIAS + F32_BIAS;
    u128_shr(&mant, &mant, 112 - 23);
    return f32_pack(sign, exp, mant.w[0]);
}

/* ========================================================================= */
/* Decimal String Parsing Helpers                                            */
/* ========================================================================= */

int soft_strto_f32(const char *str, unsigned int *out) {
    soft_f64 val;
    parse_dec_f64(str, &val);
    *out = f64_to_f32_impl(&val);
    return 0;
}

int soft_strto_f128(const char *str, soft_f128 *out) {
    soft_f64 val;
    parse_dec_f64(str, &val);
#if defined(TARGET_I386) || defined(__i386__) || defined(TARGET_RISCV32) || defined(__riscv) || defined(TARGET_32BIT)
    __extenddftf2(out, &val);
#else
    __extenddftf2(out, ((unsigned long)val.hi << 32) | (unsigned long)val.lo);
#endif
    return 0;
}
