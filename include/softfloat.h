/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef SOFTFLOAT_H
#define SOFTFLOAT_H

#if defined(TARGET_RISCV32) || defined(__riscv) || defined(__riscv__)
#include <stddef.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

/* ========================================================================= */
/* Software Floating Point Types and Multi-Word Structs                      */
/* ========================================================================= */

typedef unsigned int soft_f32;

typedef struct soft_f64 {
    unsigned int lo;
    unsigned int hi;
} soft_f64;

typedef struct soft_f128 {
    unsigned int w[4]; /* w[0]=lowest 32 bits, w[3]=highest 32 bits */
} soft_f128;

/* ========================================================================= */
/* Float32 (Single Precision) API                                            */
/* ========================================================================= */

unsigned int __addsf3(unsigned int a, unsigned int b);
unsigned int __subsf3(unsigned int a, unsigned int b);
unsigned int __mulsf3(unsigned int a, unsigned int b);
unsigned int __divsf3(unsigned int a, unsigned int b);
unsigned int __negsf2(unsigned int a);

int __eqsf2(unsigned int a, unsigned int b);
int __nesf2(unsigned int a, unsigned int b);
int __ltsf2(unsigned int a, unsigned int b);
int __lesf2(unsigned int a, unsigned int b);
int __gtsf2(unsigned int a, unsigned int b);
int __gesf2(unsigned int a, unsigned int b);

unsigned int __floatsisf(int i);
unsigned int __floatdisf(long l);
unsigned int __floatunsisf(unsigned int u);
unsigned int __floatundisf(unsigned long u);

int __fixsfsi(unsigned int a);
long __fixsfdi(unsigned int a);
unsigned int __fixunssfsi(unsigned int a);
unsigned long __fixunssfdi(unsigned int a);

/* ========================================================================= */
/* Float64 (Double Precision) API                                            */
/* ========================================================================= */

#if defined(TARGET_I386) || defined(__i386__) || defined(TARGET_RISCV32) || defined(__riscv) || defined(__riscv__) || defined(TARGET_32BIT)
void *__adddf3(void *res, const void *a, const void *b);
void *__subdf3(void *res, const void *a, const void *b);
void *__muldf3(void *res, const void *a, const void *b);
void *__divdf3(void *res, const void *a, const void *b);
void *__negdf2(void *res, const void *a);

int __eqdf2(const void *a, const void *b);
int __nedf2(const void *a, const void *b);
int __ltdf2(const void *a, const void *b);
int __ledf2(const void *a, const void *b);
int __gtdf2(const void *a, const void *b);
int __gedf2(const void *a, const void *b);

void *__floatsidf(void *res, int i);
void *__floatdidf(void *res, long l);
void *__floatunsidf(void *res, unsigned int u);
void *__floatundidf(void *res, unsigned long u);

int __fixdfsi(const void *a);
long __fixdfdi(const void *a);
unsigned int __fixunsdfsi(const void *a);
unsigned long __fixunsdfdi(const void *a);

void *__extendsfdf2(void *res, unsigned int a);
unsigned int __truncdfsf2(const void *a);
void *__extenddftf2(void *res, const void *a);
void *__trunctfdf2(void *res, const void *a);

int soft_strto_f64(const char *str, void *out);
#else
unsigned long __adddf3(unsigned long a, unsigned long b);
unsigned long __subdf3(unsigned long a, unsigned long b);
unsigned long __muldf3(unsigned long a, unsigned long b);
unsigned long __divdf3(unsigned long a, unsigned long b);
unsigned long __negdf2(unsigned long a);

int __eqdf2(unsigned long a, unsigned long b);
int __nedf2(unsigned long a, unsigned long b);
int __ltdf2(unsigned long a, unsigned long b);
int __ledf2(unsigned long a, unsigned long b);
int __gtdf2(unsigned long a, unsigned long b);
int __gedf2(unsigned long a, unsigned long b);

unsigned long __floatsidf(int i);
unsigned long __floatdidf(long l);
unsigned long __floatunsidf(unsigned int u);
unsigned long __floatundidf(unsigned long u);

int __fixdfsi(unsigned long a);
long __fixdfdi(unsigned long a);
unsigned int __fixunsdfsi(unsigned long a);
unsigned long __fixunsdfdi(unsigned long a);

unsigned long __extendsfdf2(unsigned int a);
unsigned int __truncdfsf2(unsigned long a);
void *__extenddftf2(void *res, unsigned long a);
unsigned long __trunctfdf2(const void *a);

int soft_strto_f64(const char *str, void *out);
#endif

/* ========================================================================= */
/* Float128 (Quad Precision / 128-bit Long Double) API                       */
/* ========================================================================= */

void *__addtf3(void *res, const void *a, const void *b);
void *__subtf3(void *res, const void *a, const void *b);
void *__multf3(void *res, const void *a, const void *b);
void *__divtf3(void *res, const void *a, const void *b);
void *__negtf2(void *res, const void *a);

int __eqtf2(const void *a, const void *b);
int __netf2(const void *a, const void *b);
int __lttf2(const void *a, const void *b);
int __letf2(const void *a, const void *b);
int __gttf2(const void *a, const void *b);
int __getf2(const void *a, const void *b);

void *__floatsitf(void *res, int i);
void *__floatditf(void *res, long l);
void *__floatunsitf(void *res, unsigned int u);
void *__floatunditf(void *res, unsigned long u);

int __fixtfsi(const void *a);
long __fixtfdi(const void *a);
unsigned int __fixunstfsi(const void *a);
unsigned long __fixunstfdi(const void *a);

void *__extendsftf2(void *res, unsigned int a);
unsigned int __trunctfsf2(const void *a);

/* Conversion & Formatting Helpers */
int soft_strto_f32(const char *str, unsigned int *out);
int soft_strto_f128(const char *str, soft_f128 *out);

#endif /* SOFTFLOAT_H */
