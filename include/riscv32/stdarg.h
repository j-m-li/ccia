/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef _STDARG_H
#define _STDARG_H

#if defined(__CCIA__) || defined(__CC90__)

typedef char *va_list;

#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)
#define va_copy(dst, src)   __builtin_va_copy(dst, src)
#define __va_copy(dst, src) __builtin_va_copy(dst, src)

#elif defined(__clang__) || defined(__GNUC__)

typedef __builtin_va_list va_list;

#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)
#define va_copy(dst, src)   __builtin_va_copy(dst, src)
#define __va_copy(dst, src) __builtin_va_copy(dst, src)

#else

typedef char *va_list;

#define va_start(ap, last)  ((ap) = (va_list)&(last) + sizeof(last))
#define va_arg(ap, type)    (*(type *)(((ap) += sizeof(type)) - sizeof(type)))
#define va_end(ap)          ((void)0)
#define va_copy(dst, src)   ((dst) = (src))
#define __va_copy(dst, src) ((dst) = (src))

#endif

#endif /* _STDARG_H */
