/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* File access modes */
#define O_ACCMODE   00000003
#define O_RDONLY    00000000
#define O_WRONLY    00000001
#define O_RDWR      00000002

/* File creation and status flags */
#define O_CREAT        00000100
#define O_EXCL         00000200
#define O_NOCTTY       00000400
#define O_TRUNC        00001000
#define O_APPEND       00002000
#define O_NONBLOCK     00004000
#define O_NDELAY       O_NONBLOCK
#define O_SYNC         00010000
#define O_FSYNC        O_SYNC
#define O_ASYNC        00020000
#define O_DIRECT       00040000
#define O_DIRECTORY    00200000
#define O_NOFOLLOW     00400000
#define O_CLOEXEC      02000000
#define O_LARGEFILE    00100000

/* Special directory file descriptors for *at functions */
#define AT_FDCWD            -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_NO_AUTOMOUNT     0x800
#define AT_EMPTY_PATH       0x1000

/* fcntl commands */
#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_GETLK         5
#define F_SETLK         6
#define F_SETLKW        7
#define F_SETOWN        8
#define F_GETOWN        9
#define F_SETSIG        10
#define F_GETSIG        11
#define F_GETLK64       12
#define F_SETLK64       13
#define F_SETLKW64      14
#define F_DUPFD_CLOEXEC 1030

#define FD_CLOEXEC      1

/* File lock types */
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

struct flock64 {
    short l_type;
    short l_whence;
    off64_t l_start;
    off64_t l_len;
    pid_t l_pid;
};

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

#endif /* _FCNTL_H */
