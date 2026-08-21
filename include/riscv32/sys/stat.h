/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

/* File type masks and values */
#define S_IFMT   0170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000

#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

/* File permission bits */
#define S_ISUID  04000
#define S_ISGID  02000
#define S_ISVTX  01000

#define S_IRWXU  0700
#define S_IRUSR  0400
#define S_IWUSR  0200
#define S_IXUSR  0100

#define S_IRWXG  0070
#define S_IRGRP  0040
#define S_IWGRP  0020
#define S_IXGRP  0010

#define S_IRWXO  0007
#define S_IROTH  0004
#define S_IWOTH  0002
#define S_IXOTH  0001

#if defined(__riscv) || defined(__riscv__)
/* Linux RV32 (asm-generic) struct stat (80 bytes) */
struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    mode_t        st_mode;
    nlink_t       st_nlink;
    uid_t         st_uid;
    gid_t         st_gid;
    unsigned long st_rdev;
    unsigned long __pad1;
    off_t         st_size;
    blksize_t     st_blksize;
    int           __pad2;
    blkcnt_t      st_blocks;
    time_t        st_atime;
    unsigned long st_atime_nsec;
    time_t        st_mtime;
    unsigned long st_mtime_nsec;
    time_t        st_ctime;
    unsigned long st_ctime_nsec;
    unsigned int  __unused4;
    unsigned int  __unused5;
};
#elif defined(__i386__) || defined(__i386)
/* Linux i386 (glibc) struct stat (88 bytes) */
struct stat {
    unsigned long st_dev;
    unsigned long __pad_dev;
    unsigned int  __pad0;
    unsigned long st_ino;
    mode_t        st_mode;
    nlink_t       st_nlink;
    uid_t         st_uid;
    gid_t         st_gid;
    unsigned long st_rdev;
    unsigned long __pad_rdev;
    unsigned int  __pad1;
    off_t         st_size;
    blksize_t     st_blksize;
    blkcnt_t      st_blocks;
    time_t        st_atime;
    unsigned long st_atime_nsec;
    time_t        st_mtime;
    unsigned long st_mtime_nsec;
    time_t        st_ctime;
    unsigned long st_ctime_nsec;
    unsigned int  __unused[2];
};
#else
/* Linux x86_64 (glibc) struct stat (144 bytes) */
struct stat {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    unsigned int  st_mode;
    unsigned int  st_uid;
    unsigned int  st_gid;
    unsigned int  __pad0;
    unsigned long st_rdev;
    long          st_size;
    long          st_blksize;
    long          st_blocks;
    long          st_atime;
    unsigned long st_atime_nsec;
    long          st_mtime;
    unsigned long st_mtime_nsec;
    long          st_ctime;
    unsigned long st_ctime_nsec;
    long          __glibc_reserved[3];
};
#endif

int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *path, struct stat *buf);
int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int mkdir(const char *path, mode_t mode);

#endif /* _SYS_STAT_H */
