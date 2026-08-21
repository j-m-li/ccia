/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef int ssize_t;
typedef long off_t;
typedef long long off64_t;
typedef long long loff_t;

typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long long dev_t;
typedef unsigned long ino_t;
typedef unsigned long long ino64_t;
typedef unsigned int mode_t;
typedef unsigned int nlink_t;

typedef long time_t;
typedef long suseconds_t;
typedef long clock_t;

typedef long blksize_t;
typedef long blkcnt_t;
typedef long long blkcnt64_t;
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

typedef char *caddr_t;
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

typedef int daddr_t;
typedef int key_t;

#endif /* _SYS_TYPES_H */
