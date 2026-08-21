/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <sys/types.h>

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Seek whence constants */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* access() mode constants */
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

/* File I/O and descriptor management */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
int fsync(int fd);
int fdatasync(int fd);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
int isatty(int fd);
char *ttyname(int fd);

/* File system operations */
int unlink(const char *pathname);
int unlinkat(int dirfd, const char *pathname, int flags);
int rmdir(const char *pathname);
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int chdir(const char *path);
int fchdir(int fd);
char *getcwd(char *buf, size_t size);
int access(const char *pathname, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);

/* Process management and identification */
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);
int seteuid(uid_t euid);
int setegid(gid_t egid);
int execv(const char *path, char *const argv[]);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
void _exit(int status);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned long usec);
int pause(void);

/* Memory management & System configuration */
int brk(void *addr);
void *sbrk(intptr_t increment);
long sysconf(int name);

#endif /* _UNISTD_H */
