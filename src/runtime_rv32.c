/*
 * This is free and unencumbered software released into the public domain.
 * See the UNLICENSE file or http://unlicense.org/ for details.
 */

#if defined(TARGET_RISCV32) || defined(__riscv) || defined(__riscv__) || defined(__CCIA_RISCV32__)

#include <stddef.h>
#include <stdarg.h>

typedef struct FILE FILE;

int isspace(int c);
int isdigit(int c);
int isalpha(int c);
int isupper(int c);
int islower(int c);
int tolower(int c);
int toupper(int c);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

int errno = 0;

/* ========================================================================= */
/* Linux RISC-V 32-bit System Call Numbers                                   */
/* ========================================================================= */

#define SYS_getcwd      17
#define SYS_dup         23
#define SYS_dup3        24
#define SYS_fcntl       25
#define SYS_ioctl       29
#define SYS_unlinkat    35
#define SYS_openat      56
#define SYS_close       57
#define SYS_lseek       62
#define SYS_read        63
#define SYS_write       64
#define SYS_readv       65
#define SYS_writev      66
#define SYS_fstat       80
#define SYS_exit        93
#define SYS_exit_group  94
#define SYS_kill        129
#define SYS_brk         214
#define SYS_munmap      215
#define SYS_clone       220
#define SYS_execve      221
#define SYS_mmap        222
#define SYS_wait4       260

#define AT_FDCWD        -100
#define O_RDONLY        0
#define O_WRONLY        1
#define O_RDWR          2
#define O_CREAT         0100
#define O_TRUNC         01000
#define O_APPEND        02000

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2

/* ========================================================================= */
/* System Call Wrappers using RV32 ecall                                     */
/* ========================================================================= */

static long sys_call0(long num) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = 0;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7) : "memory");
    return r_a0;
}

static long sys_call1(long num, long a0) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = a0;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7) : "memory");
    return r_a0;
}

static long sys_call2(long num, long a0, long a1) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = a0;
    register long r_a1 __asm__("a1") = a1;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7), "r"(r_a1) : "memory");
    return r_a0;
}

static long sys_call3(long num, long a0, long a1, long a2) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = a0;
    register long r_a1 __asm__("a1") = a1;
    register long r_a2 __asm__("a2") = a2;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7), "r"(r_a1), "r"(r_a2) : "memory");
    return r_a0;
}

static long sys_call4(long num, long a0, long a1, long a2, long a3) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = a0;
    register long r_a1 __asm__("a1") = a1;
    register long r_a2 __asm__("a2") = a2;
    register long r_a3 __asm__("a3") = a3;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7), "r"(r_a1), "r"(r_a2), "r"(r_a3) : "memory");
    return r_a0;
}

static long sys_call5(long num, long a0, long a1, long a2, long a3, long a4) {
    register long r_a7 __asm__("a7") = num;
    register long r_a0 __asm__("a0") = a0;
    register long r_a1 __asm__("a1") = a1;
    register long r_a2 __asm__("a2") = a2;
    register long r_a3 __asm__("a3") = a3;
    register long r_a4 __asm__("a4") = a4;
    __asm__ volatile ("ecall" : "+r"(r_a0) : "r"(r_a7), "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4) : "memory");
    return r_a0;
}

/* ========================================================================= */
/* Process Exit & Entry Point                                                */
/* ========================================================================= */

void exit(int status) {
    sys_call1(SYS_exit_group, status);
    sys_call1(SYS_exit, status);
    for (;;) {}
}

void abort(void) {
    exit(134);
}

char **environ = NULL;

extern int main(int argc, char **argv);

void _start_c(int argc, char **argv, char **envp) {
    environ = envp;
    exit(main(argc, argv));
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile (
        "lw a0, 0(sp)\n"
        "addi a1, sp, 4\n"
        "slli a2, a0, 2\n"
        "add a2, a2, a1\n"
        "addi a2, a2, 4\n"
        "addi sp, sp, -16\n"
        "call _start_c\n"
    );
}

/* ========================================================================= */
/* Memory Allocation (brk-based heap allocator)                              */
/* ========================================================================= */

static void *heap_curr = NULL;

void *malloc(size_t size) {
    void *prev;
    size_t aligned_size;
    if (size == 0) return NULL;
    aligned_size = (size + 15) & ~15; /* 16-byte alignment */

    if (!heap_curr) {
        heap_curr = (void *)sys_call1(SYS_brk, 0);
    }
    prev = heap_curr;
    heap_curr = (void *)sys_call1(SYS_brk, (long)heap_curr + (long)aligned_size);
    if (heap_curr == prev) {
        return NULL; /* Out of memory */
    }
    return prev;
}

void free(void *ptr) {
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) {
        char *b = (char *)p;
        size_t i;
        for (i = 0; i < total; i++) b[i] = 0;
    }
    return p;
}

void *realloc(void *ptr, size_t size) {
    void *new_p;
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return NULL; }
    new_p = malloc(size);
    if (new_p && ptr) {
        /* Simple copy of size bytes */
        char *d = (char *)new_p;
        char *s = (char *)ptr;
        size_t i;
        for (i = 0; i < size; i++) d[i] = s[i];
    }
    return new_p;
}

/* ========================================================================= */
/* Basic Memory & String Library                                             */
/* ========================================================================= */

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    size_t i;
    for (i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    size_t i;
    for (i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    size_t i;
    if (d < s) {
        for (i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    size_t i;
    for (i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] == (unsigned char)c) return (void *)(p + i);
    }
    return NULL;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++)) ;
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i;
    while (*d) d++;
    for (i = 0; i < n && src[i]; i++) *d++ = src[i];
    *d = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (s1[i] != s2[i] || !s1[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if ((char)c == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    size_t nlen;
    if (!*needle) return (char *)haystack;
    nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    while (*s && strchr(accept, *s)) {
        count++;
        s++;
    }
    return count;
}

size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    while (*s && !strchr(reject, *s)) {
        count++;
        s++;
    }
    return count;
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *p = (char *)malloc(len + 1);
    if (p) strcpy(p, s);
    return p;
}

char *strerror(int errnum) {
    (void)errnum;
    return "unknown error";
}

int abs(int x) {
    return x < 0 ? -x : x;
}

int atoi(const char *nptr) {
    int res = 0;
    int sign = 1;
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n' || *nptr == '\r') nptr++;
    if (*nptr == '-') { sign = -1; nptr++; }
    else if (*nptr == '+') { nptr++; }
    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    return sign * res;
}

long atol(const char *nptr) {
    return (long)atoi(nptr);
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc = 0;
    int c;
    unsigned long cutoff;
    int neg = 0, any = 0, cutlim;

    while (isspace((unsigned char)*s)) s++;
    if (*s == '-') {
        neg = 1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }
    cutoff = neg ? 0x80000000UL : 0x7FFFFFFFUL;
    cutlim = cutoff % (unsigned long)base;
    cutoff /= (unsigned long)base;
    for (;; s++) {
        c = (unsigned char)*s;
        if (isdigit(c)) c -= '0';
        else if (isalpha(c)) c = tolower(c) - 'a' + 10;
        else break;
        if (c >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && (unsigned long)c > (unsigned long)cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * base + c;
        }
    }
    if (any < 0) {
        acc = neg ? 0x80000000UL : 0x7FFFFFFFUL;
    } else if (neg) {
        acc = -acc;
    }
    if (endptr != NULL) {
        *endptr = (char *)(any ? s : nptr);
    }
    return (long)acc;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long acc = 0;
    int c;
    unsigned long cutoff;
    int any = 0, cutlim;

    while (isspace((unsigned char)*s)) s++;
    if (*s == '+') s++;
    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    }
    if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }
    cutoff = 0xFFFFFFFFUL / (unsigned long)base;
    cutlim = 0xFFFFFFFFUL % (unsigned long)base;
    for (;; s++) {
        c = (unsigned char)*s;
        if (isdigit(c)) c -= '0';
        else if (isalpha(c)) c = tolower(c) - 'a' + 10;
        else break;
        if (c >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && (unsigned long)c > (unsigned long)cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * base + c;
        }
    }
    if (any < 0) {
        acc = 0xFFFFFFFFUL;
    }
    if (endptr != NULL) {
        *endptr = (char *)(any ? s : nptr);
    }
    return acc;
}

/* ========================================================================= */
/* Standard I/O (FILE struct, Streams, Formatted Output)                     */
/* ========================================================================= */

typedef struct FILE {
    int fd;
    int eof;
    int error;
    int ungetc_char;
    int has_ungetc;
} FILE;

static FILE f_stdin  = { 0, 0, 0, 0, 0 };
static FILE f_stdout = { 1, 0, 0, 0, 0 };
static FILE f_stderr = { 2, 0, 0, 0, 0 };

FILE *stdin  = &f_stdin;
FILE *stdout = &f_stdout;
FILE *stderr = &f_stderr;

FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;
    int fd;
    FILE *f;

    if (strchr(mode, 'w')) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    } else if (strchr(mode, 'a')) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else if (strchr(mode, '+')) {
        flags = O_RDWR;
    } else {
        flags = O_RDONLY;
    }

    fd = sys_call4(SYS_openat, AT_FDCWD, (long)pathname, flags, 0666);
    if (fd < 0) return NULL;

    f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        sys_call1(SYS_close, fd);
        return NULL;
    }
    f->fd = fd;
    f->eof = 0;
    f->error = 0;
    f->ungetc_char = 0;
    f->has_ungetc = 0;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    if (stream->fd >= 0) {
        sys_call1(SYS_close, stream->fd);
        stream->fd = -1;
    }
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return 0;
}

int remove(const char *pathname) {
    long ret = sys_call3(SYS_unlinkat, AT_FDCWD, (long)pathname, 0);
    return (ret < 0) ? -1 : 0;
}

int ungetc(int c, FILE *stream) {
    if (!stream || c == -1) return -1;
    stream->ungetc_char = (unsigned char)c;
    stream->has_ungetc = 1;
    stream->eof = 0;
    return (unsigned char)c;
}

int fgetc(FILE *stream) {
    unsigned char ch;
    if (!stream) return -1;
    if (stream->has_ungetc) {
        stream->has_ungetc = 0;
        return stream->ungetc_char;
    }
    if (fread(&ch, 1, 1, stream) == 1) {
        return (int)ch;
    }
    return -1;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

int getchar(void) {
    return fgetc(stdin);
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    size_t read_bytes = 0;
    long ret;
    unsigned char *dst = (unsigned char *)ptr;
    if (!stream || total == 0) return 0;
    if (stream->has_ungetc && total > 0) {
        *dst++ = (unsigned char)stream->ungetc_char;
        stream->has_ungetc = 0;
        read_bytes++;
        total--;
    }
    if (total > 0) {
        ret = sys_call3(SYS_read, stream->fd, (long)dst, total);
        if (ret < 0) {
            stream->error = 1;
            return read_bytes / size;
        }
        if (ret == 0) {
            stream->eof = 1;
            return read_bytes / size;
        }
        read_bytes += (size_t)ret;
    }
    return read_bytes / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    long ret;
    if (!stream || total == 0) return 0;
    ret = sys_call3(SYS_write, stream->fd, (long)ptr, total);
    if (ret < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)ret / size;
}

int fseek(FILE *stream, long offset, int whence) {
    unsigned long long res = 0;
    long ret;
    if (!stream) return -1;
    ret = sys_call5(SYS_lseek, stream->fd, (offset < 0 ? -1L : 0L), offset, (long)&res, whence);
    if (ret < 0) {
        stream->error = 1;
        return -1;
    }
    stream->eof = 0;
    return 0;
}

long ftell(FILE *stream) {
    unsigned long long res = 0;
    long ret;
    if (!stream) return -1;
    ret = sys_call5(SYS_lseek, stream->fd, 0, 0, (long)&res, SEEK_CUR);
    if (ret < 0) {
        stream->error = 1;
        return -1;
    }
    return (long)res;
}

int feof(FILE *stream) {
    return stream ? stream->eof : 0;
}

int ferror(FILE *stream) {
    return stream ? stream->error : 0;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

int fputc(int c, FILE *stream) {
    char ch = (char)c;
    if (fwrite(&ch, 1, 1, stream) == 1) return c;
    return -1;
}

int fputs(const char *s, FILE *stream) {
    size_t len = strlen(s);
    if (fwrite(s, 1, len, stream) == len) return 0;
    return -1;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int puts(const char *s) {
    if (fputs(s, stdout) < 0) return -1;
    return fputc('\n', stdout);
}

char *fgets(char *s, int size, FILE *stream) {
    int i = 0;
    if (!s || size <= 0 || !stream) return NULL;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c == -1) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fileno(FILE *stream) {
    return stream ? stream->fd : -1;
}

int isatty(int fd) {
    char termios_buf[128];
    long r = sys_call3(SYS_ioctl, fd, 0x5401 /* TCGETS */, (long)termios_buf);
    return r == 0;
}

/* ========================================================================= */
/* Formatted Printing Helpers (vfprintf, sprintf, snprintf)                  */
/* ========================================================================= */

typedef struct FormatSink {
    FILE *fp;
    char *str;
    size_t max_len;
    size_t count;
} FormatSink;

static void sink_putc(FormatSink *s, char c) {
    if (s->fp) {
        fputc(c, s->fp);
        s->count++;
    } else if (s->str) {
        if (s->count + 1 < s->max_len) {
            s->str[s->count] = c;
        }
        s->count++;
    }
}

static void sink_puts(FormatSink *s, const char *str) {
    while (*str) sink_putc(s, *str++);
}

static void sink_put_uint(FormatSink *s, unsigned long val, int base, int uppercase, int width, char pad) {
    char buf[64];
    int len = 0;
    int pad_len;
    int i;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0) {
        buf[len++] = '0';
    } else {
        while (val > 0) {
            buf[len++] = digits[val % base];
            val /= base;
        }
    }

    pad_len = width - len;
    for (i = 0; i < pad_len; i++) sink_putc(s, pad);
    for (i = len - 1; i >= 0; i--) sink_putc(s, buf[i]);
}

static void sink_put_int(FormatSink *s, long val, int width, char pad) {
    if (val < 0) {
        sink_putc(s, '-');
        if (width > 0) width--;
        sink_put_uint(s, (unsigned long)(-val), 10, 0, width, pad);
    } else {
        sink_put_uint(s, (unsigned long)val, 10, 0, width, pad);
    }
}

static void format_core(FormatSink *sink, const char *format, va_list ap) {
    const char *p = format;
    while (*p) {
        if (*p != '%') {
            sink_putc(sink, *p++);
            continue;
        }
        p++; /* skip '%' */
        if (*p == '%') {
            sink_putc(sink, '%');
            p++;
            continue;
        }

        {
            int left_align = 0;
            char pad = ' ';
            int width = 0;
            int precision = -1;
            int is_long = 0;

            /* Flags */
            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') {
                if (*p == '-') left_align = 1;
                if (*p == '0' && !left_align) pad = '0';
                p++;
            }
            if (left_align) pad = ' ';

            /* Field width */
            if (*p == '*') {
                width = va_arg(ap, int);
                if (width < 0) {
                    left_align = 1;
                    width = -width;
                    pad = ' ';
                }
                p++;
            } else {
                while (*p >= '0' && *p <= '9') {
                    width = width * 10 + (*p - '0');
                    p++;
                }
            }

            /* Precision */
            if (*p == '.') {
                p++;
                if (*p == '*') {
                    precision = va_arg(ap, int);
                    p++;
                } else {
                    precision = 0;
                    while (*p >= '0' && *p <= '9') {
                        precision = precision * 10 + (*p - '0');
                        p++;
                    }
                }
            }

            /* Length modifier */
            if (*p == 'h') { p++; if (*p == 'h') p++; }
            else if (*p == 'l') { is_long = 1; p++; if (*p == 'l') { is_long = 2; p++; } }
            else if (*p == 'z' || *p == 't') { is_long = 1; p++; }

            if (*p == 'd' || *p == 'i') {
                long val = (is_long >= 1) ? (long)va_arg(ap, long) : (long)va_arg(ap, int);
                char buf[64];
                int len = 0;
                int i;
                int is_neg = (val < 0);
                unsigned long uval = is_neg ? (unsigned long)(-val) : (unsigned long)val;
                if (uval == 0) buf[len++] = '0';
                else {
                    while (uval > 0) {
                        buf[len++] = '0' + (uval % 10);
                        uval /= 10;
                    }
                }
                if (is_neg) len++; /* for '-' */
                if (!left_align && pad == ' ' && width > len) {
                    for (i = 0; i < width - len; i++) sink_putc(sink, ' ');
                }
                if (is_neg) sink_putc(sink, '-');
                if (!left_align && pad == '0' && width > len) {
                    for (i = 0; i < width - len; i++) sink_putc(sink, '0');
                }
                for (i = (is_neg ? len - 2 : len - 1); i >= 0; i--) sink_putc(sink, buf[i]);
                if (left_align && width > len) {
                    for (i = 0; i < width - len; i++) sink_putc(sink, ' ');
                }
            } else if (*p == 'u' || *p == 'x' || *p == 'X' || *p == 'o') {
                unsigned long val = (is_long >= 1) ? va_arg(ap, unsigned long) : (unsigned long)va_arg(ap, unsigned int);
                int base = (*p == 'o') ? 8 : ((*p == 'u') ? 10 : 16);
                int uppercase = (*p == 'X');
                char buf[64];
                int len = 0;
                int i;
                const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
                if (val == 0) buf[len++] = '0';
                else {
                    while (val > 0) {
                        buf[len++] = digits[val % base];
                        val /= base;
                    }
                }
                if (!left_align && width > len) {
                    for (i = 0; i < width - len; i++) sink_putc(sink, pad);
                }
                for (i = len - 1; i >= 0; i--) sink_putc(sink, buf[i]);
                if (left_align && width > len) {
                    for (i = 0; i < width - len; i++) sink_putc(sink, ' ');
                }
            } else if (*p == 'p') {
                void *ptr = va_arg(ap, void *);
                sink_puts(sink, "0x");
                sink_put_uint(sink, (unsigned long)ptr, 16, 0, 0, ' ');
            } else if (*p == 's') {
                const char *s = va_arg(ap, const char *);
                int slen, i;
                if (!s) s = "(null)";
                slen = (int)strlen(s);
                if (precision >= 0 && precision < slen) slen = precision;
                if (!left_align && width > slen) {
                    for (i = 0; i < width - slen; i++) sink_putc(sink, ' ');
                }
                for (i = 0; i < slen; i++) sink_putc(sink, s[i]);
                if (left_align && width > slen) {
                    for (i = 0; i < width - slen; i++) sink_putc(sink, ' ');
                }
            } else if (*p == 'c') {
                int ch = va_arg(ap, int);
                sink_putc(sink, (char)ch);
            } else {
                sink_putc(sink, *p);
            }
            if (*p) p++;
        }
    }
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    FormatSink sink;
    sink.fp = stream;
    sink.str = NULL;
    sink.max_len = 0;
    sink.count = 0;
    format_core(&sink, format, ap);
    return (int)sink.count;
}

int printf(const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vfprintf(stdout, format, ap);
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    FormatSink sink;
    sink.fp = NULL;
    sink.str = str;
    sink.max_len = size;
    sink.count = 0;
    format_core(&sink, format, ap);
    if (size > 0 && str) {
        if (sink.count < size) str[sink.count] = '\0';
        else str[size - 1] = '\0';
    }
    return (int)sink.count;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    int ret;
    va_start(ap, format);
    ret = vsnprintf(str, 1000000, format, ap);
    va_end(ap);
    return ret;
}

/* ========================================================================= */
/* Quick Sort (qsort)                                                        */
/* ========================================================================= */

static void swap_bytes(char *a, char *b, size_t size) {
    size_t i;
    for (i = 0; i < size; i++) {
        char tmp = a[i];
        a[i] = b[i];
        b[i] = tmp;
    }
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    char *arr = (char *)base;
    size_t i, j, p;
    if (nmemb < 2 || size == 0) return;
    p = nmemb / 2;
    swap_bytes(arr + p * size, arr + (nmemb - 1) * size, size);
    i = 0;
    for (j = 0; j < nmemb - 1; j++) {
        if (compar(arr + j * size, arr + (nmemb - 1) * size) <= 0) {
            swap_bytes(arr + i * size, arr + j * size, size);
            i++;
        }
    }
    swap_bytes(arr + i * size, arr + (nmemb - 1) * size, size);
    qsort(arr, i, size, compar);
    qsort(arr + (i + 1) * size, nmemb - i - 1, size, compar);
}

/* ========================================================================= */
/* Soft-Integer Multiplication, Division & Modulo (RV32I Software Routines)  */
/* ========================================================================= */

int __mulsi3(int a, int b) {
    unsigned int ua = (unsigned int)a;
    unsigned int ub = (unsigned int)b;
    unsigned int res = 0;
    while (ub > 0) {
        if (ub & 1) res += ua;
        ua <<= 1;
        ub >>= 1;
    }
    return (int)res;
}

unsigned int __udivsi3(unsigned int a, unsigned int b) {
    unsigned int q = 0;
    unsigned int r = 0;
    int i;
    if (b == 0) return 0;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
            q |= (1U << i);
        }
    }
    return q;
}

unsigned int __umodsi3(unsigned int a, unsigned int b) {
    unsigned int r = 0;
    int i;
    if (b == 0) return 0;
    for (i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
        }
    }
    return r;
}

int __divsi3(int a, int b) {
    int sign = 1;
    unsigned int ua, ub, q;
    if (a < 0) { sign = -sign; ua = (unsigned int)(-a); }
    else ua = (unsigned int)a;
    if (b < 0) { sign = -sign; ub = (unsigned int)(-b); }
    else ub = (unsigned int)b;
    q = __udivsi3(ua, ub);
    return sign < 0 ? (int)(-q) : (int)q;
}

int __modsi3(int a, int b) {
    int sign = 1;
    unsigned int ua, ub, r;
    if (a < 0) { sign = -sign; ua = (unsigned int)(-a); }
    else ua = (unsigned int)a;
    if (b < 0) ub = (unsigned int)(-b);
    else ub = (unsigned int)b;
    r = __umodsi3(ua, ub);
    return sign < 0 ? (int)(-r) : (int)r;
}

/* ========================================================================= */
/* Additional Standard Library Functions                                     */
/* ========================================================================= */

void __assert_fail(const char *expr, const char *file, int line, const char *func) {
    (void)func;
    fprintf(stderr, "Assertion failed: %s (%s:%d)\n", expr ? expr : "", file ? file : "", line);
    abort();
}

long labs(long j) {
    return j < 0 ? -j : j;
}

char *getenv(const char *name) {
    size_t len;
    char **ep;
    if (!name || !environ) return NULL;
    len = strlen(name);
    for (ep = environ; *ep; ep++) {
        if (strncmp(*ep, name, len) == 0 && (*ep)[len] == '=') {
            return *ep + len + 1;
        }
    }
    return NULL;
}

#define SYS_waitid      95

int waitpid(int pid, int *status, int options);

int system(const char *command) {
    long pid;
    int status = 0;

    if (!command) {
        return 1;
    }

    pid = sys_call5(SYS_clone, 17 /* SIGCHLD */, 0, 0, 0, 0);
    if (pid < 0) {
        errno = (int)(-pid);
        return -1;
    }

    if (pid == 0) {
        char *argv[4];
        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = (char *)command;
        argv[3] = NULL;
        sys_call3(SYS_execve, (long)"/bin/sh", (long)argv, (long)environ);
        sys_call3(SYS_execve, (long)"/usr/bin/sh", (long)argv, (long)environ);
        sys_call1(SYS_exit, 127);
    }

    if (waitpid((int)pid, &status, 0) < 0) {
        return -1;
    }

    return status;
}

/* Character classification & conversion */
int isalnum(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int iscntrl(int c) { return (c >= 0 && c <= 31) || c == 127; }
int isdigit(int c) { return (c >= '0' && c <= '9'); }
int isgraph(int c) { return (c >= 33 && c <= 126); }
int islower(int c) { return (c >= 'a' && c <= 'z'); }
int isprint(int c) { return (c >= 32 && c <= 126); }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int tolower(int c) { return isupper(c) ? (c + ('a' - 'A')) : c; }
int toupper(int c) { return islower(c) ? (c - ('a' - 'A')) : c; }

/* Math library functions */
double fabs(double x) {
    return x < 0.0 ? -x : x;
}

double ldexp(double x, int exp) {
    while (exp > 0) { x *= 2.0; exp--; }
    while (exp < 0) { x *= 0.5; exp++; }
    return x;
}

double frexp(double value, int *exp) {
    int e = 0;
    double v = value;
    if (value == 0.0) {
        if (exp) *exp = 0;
        return 0.0;
    }
    if (v < 0.0) v = -v;
    while (v >= 1.0) {
        v *= 0.5;
        e++;
    }
    while (v < 0.5) {
        v *= 2.0;
        e--;
    }
    if (exp) *exp = e;
    return (value < 0.0) ? -v : v;
}

double ceil(double x) {
    long i = (long)x;
    if (x > (double)i) return (double)(i + 1);
    return (double)i;
}

double floor(double x) {
    long i = (long)x;
    if (x < (double)i) return (double)(i - 1);
    return (double)i;
}

double sqrt(double x) {
    double guess;
    int i;
    if (x <= 0.0) return 0.0;
    guess = x > 1.0 ? x * 0.5 : 1.0;
    for (i = 0; i < 20; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

double pow(double x, double y) {
    double res = 1.0;
    long exp = (long)y;
    int neg = 0;
    if (y == 0.0) return 1.0;
    if (exp < 0) { neg = 1; exp = -exp; }
    while (exp > 0) {
        if (exp & 1) res *= x;
        x *= x;
        exp >>= 1;
    }
    return neg ? (1.0 / res) : res;
}

double sin(double x) {
    /* Taylor series approximation */
    double term = x;
    double sum = x;
    int i;
    for (i = 1; i <= 7; i++) {
        term = -term * x * x / ((2 * i) * (2 * i + 1));
        sum += term;
    }
    return sum;
}

double cos(double x) {
    double term = 1.0;
    double sum = 1.0;
    int i;
    for (i = 1; i <= 7; i++) {
        term = -term * x * x / ((2 * i - 1) * (2 * i));
        sum += term;
    }
    return sum;
}

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
    (void)signum;
    (void)handler;
    return (sighandler_t)0;
}

int raise(int sig) {
    (void)sig;
    return 0;
}

int kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    return 0;
}

struct passwd {
    char *pw_name;
    char *pw_passwd;
    unsigned int pw_uid;
    unsigned int pw_gid;
    char *pw_gecos;
    char *pw_dir;
    char *pw_shell;
};

struct passwd *getpwuid(unsigned int uid) {
    (void)uid;
    return (struct passwd *)0;
}

struct passwd *getpwnam(const char *name) {
    (void)name;
    return (struct passwd *)0;
}

int open(const char *pathname, int flags, ...) {
    int mode = 0;
    long ret;
    if (flags & 0100 /* O_CREAT */) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, int);
        va_end(ap);
    }
    ret = sys_call4(SYS_openat, AT_FDCWD, (long)pathname, flags, mode);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (int)ret;
}

int close(int fd) {
    long ret = sys_call1(SYS_close, fd);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

long read(int fd, void *buf, size_t count) {
    long ret = sys_call3(SYS_read, fd, (long)buf, count);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return ret;
}

long write(int fd, const void *buf, size_t count) {
    long ret = sys_call3(SYS_write, fd, (long)buf, count);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return ret;
}

long lseek(int fd, long offset, int whence) {
    unsigned long long res = 0;
    long ret = sys_call5(SYS_lseek, fd, (long)((unsigned long long)offset >> 32), (long)(offset & 0xFFFFFFFF), (long)&res, whence);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return (long)res;
}

int unlink(const char *pathname) {
    long ret = sys_call3(SYS_unlinkat, AT_FDCWD, (long)pathname, 0);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int access(const char *pathname, int mode) {
    long ret = sys_call3(48 /* SYS_faccessat */, AT_FDCWD, (long)pathname, mode);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

struct statx_ts_t {
    long long tv_sec;
    unsigned int tv_nsec;
    int __reserved;
};
struct statx_t {
    unsigned int stx_mask;
    unsigned int stx_blksize;
    unsigned long long stx_attributes;
    unsigned int stx_nlink;
    unsigned int stx_uid;
    unsigned int stx_gid;
    unsigned short stx_mode;
    unsigned short __spare0[1];
    unsigned long long stx_ino;
    unsigned long long stx_size;
    unsigned long long stx_blocks;
    unsigned long long stx_attributes_mask;
    struct statx_ts_t stx_atime;
    struct statx_ts_t stx_btime;
    struct statx_ts_t stx_ctime;
    struct statx_ts_t stx_mtime;
    unsigned int stx_rdev_major;
    unsigned int stx_rdev_minor;
    unsigned int stx_dev_major;
    unsigned int stx_dev_minor;
    unsigned long long stx_mnt_id;
    unsigned long long __spare2[13];
};

struct stat_rv32 {
    unsigned int st_dev;
    unsigned int st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned int st_rdev;
    unsigned int __pad1;
    long         st_size;
    long         st_blksize;
    int          __pad2;
    long         st_blocks;
    long         st_atime;
    unsigned int st_atime_nsec;
    long         st_mtime;
    unsigned int st_mtime_nsec;
    long         st_ctime;
    unsigned int st_ctime_nsec;
    unsigned int __unused4;
    unsigned int __unused5;
};

static void copy_statx_to_stat(const struct statx_t *sx, void *buf) {
    struct stat_rv32 *st = (struct stat_rv32 *)buf;
    if (!st) return;
    memset(st, 0, sizeof(*st));
    st->st_dev = ((unsigned int)sx->stx_dev_major << 8) | sx->stx_dev_minor;
    st->st_ino = (unsigned int)sx->stx_ino;
    st->st_mode = sx->stx_mode;
    st->st_nlink = sx->stx_nlink;
    st->st_uid = sx->stx_uid;
    st->st_gid = sx->stx_gid;
    st->st_rdev = ((unsigned int)sx->stx_rdev_major << 8) | sx->stx_rdev_minor;
    st->st_size = (long)sx->stx_size;
    st->st_blksize = (long)sx->stx_blksize;
    st->st_blocks = (long)sx->stx_blocks;
    st->st_atime = (long)sx->stx_atime.tv_sec;
    st->st_atime_nsec = sx->stx_atime.tv_nsec;
    st->st_mtime = (long)sx->stx_mtime.tv_sec;
    st->st_mtime_nsec = sx->stx_mtime.tv_nsec;
    st->st_ctime = (long)sx->stx_ctime.tv_sec;
    st->st_ctime_nsec = sx->stx_ctime.tv_nsec;
}

int fstat(int fd, void *buf) {
    struct statx_t sx;
    long ret = sys_call5(291 /* SYS_statx */, fd, (long)"", 0x1000 /* AT_EMPTY_PATH */, 0x7ff, (long)&sx);
    if (ret == 0) {
        copy_statx_to_stat(&sx, buf);
        return 0;
    }
    ret = sys_call2(SYS_fstat, fd, (long)buf);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int stat(const char *pathname, void *buf) {
    struct statx_t sx;
    long ret = sys_call5(291 /* SYS_statx */, AT_FDCWD, (long)pathname, 0, 0x7ff, (long)&sx);
    if (ret == 0) {
        copy_statx_to_stat(&sx, buf);
        return 0;
    }
    ret = sys_call4(79 /* SYS_fstatat */, AT_FDCWD, (long)pathname, (long)buf, 0);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int lstat(const char *pathname, void *buf) {
    struct statx_t sx;
    long ret = sys_call5(291 /* SYS_statx */, AT_FDCWD, (long)pathname, 0x100 /* AT_SYMLINK_NOFOLLOW */, 0x7ff, (long)&sx);
    if (ret == 0) {
        copy_statx_to_stat(&sx, buf);
        return 0;
    }
    ret = sys_call4(79 /* SYS_fstatat */, AT_FDCWD, (long)pathname, (long)buf, 0x100 /* AT_SYMLINK_NOFOLLOW */);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int getpid(void) {
    return (int)sys_call0(172 /* SYS_getpid */);
}

int getuid(void) {
    return (int)sys_call0(174 /* SYS_getuid */);
}

int geteuid(void) {
    return (int)sys_call0(175 /* SYS_geteuid */);
}

int getgid(void) {
    return (int)sys_call0(176 /* SYS_getgid */);
}

int getegid(void) {
    return (int)sys_call0(177 /* SYS_getegid */);
}

int fork(void) {
    long pid = sys_call5(SYS_clone, 17 /* SIGCHLD */, 0, 0, 0, 0);
    if (pid < 0) {
        errno = (int)(-pid);
        return -1;
    }
    return (int)pid;
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    long ret = sys_call3(SYS_execve, (long)pathname, (long)argv, (long)(envp ? envp : environ));
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int execv(const char *path, char *const argv[]) {
    return execve(path, argv, environ);
}

void _exit(int status) {
    sys_call1(SYS_exit_group, status);
    sys_call1(SYS_exit, status);
    for (;;) {}
}

int waitpid(int pid, int *status, int options) {
    int siginfo[32];
    int i;
    long ret;
    int idtype = (pid == -1) ? 0 /* P_ALL */ : (pid < -1 ? 2 /* P_PGID */ : 1 /* P_PID */);
    long id = (pid == -1) ? 0 : (pid < -1 ? -pid : pid);
    int waitid_options = 4 /* WEXITED */;
    if (options & 1 /* WNOHANG */) waitid_options |= 1 /* WNOHANG */;
    if (options & 2 /* WUNTRACED */) waitid_options |= 2 /* WSTOPPED */;

    for (i = 0; i < 32; i++) siginfo[i] = 0;

    do {
        ret = sys_call5(SYS_waitid, idtype, id, (long)siginfo, waitid_options, 0);
    } while (ret == -4 /* -EINTR */);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    if (siginfo[3] == 0) {
        return 0;
    }

    if (status) {
        int code = siginfo[2];
        int child_stat = siginfo[5];
        if (code == 1 /* CLD_EXITED */) {
            *status = (child_stat & 0xff) << 8;
        } else if (code == 2 /* CLD_KILLED */ || code == 3 /* CLD_DUMPED */) {
            *status = (child_stat & 0x7f) | (code == 3 ? 0x80 : 0);
        } else if (code == 5 /* CLD_STOPPED */) {
            *status = ((child_stat & 0xff) << 8) | 0x7f;
        } else if (code == 6 /* CLD_CONTINUED */) {
            *status = 0xffff;
        } else {
            *status = (child_stat & 0xff) << 8;
        }
    }

    return siginfo[3];
}

int wait(int *status) {
    return waitpid(-1, status, 0);
}

int fsync(int fd) {
    long ret = sys_call1(82 /* SYS_fsync */, fd);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int ftruncate(int fd, long length) {
    long ret = sys_call4(46 /* SYS_ftruncate */, fd, (long)((unsigned long long)length >> 32), (long)(length & 0xFFFFFFFF), 0);
    if (ret < 0) { errno = (int)(-ret); return -1; }
    return 0;
}

int fcntl(int fd, int cmd, ...) {
    long arg = 0;
    va_list ap;
    va_start(ap, cmd);
    arg = va_arg(ap, long);
    va_end(ap);
    return (int)sys_call3(SYS_fcntl, fd, cmd, arg);
}

char *getcwd(char *buf, size_t size) {
    long r = sys_call2(SYS_getcwd, (long)buf, size);
    if (r < 0) return NULL;
    return buf;
}

int usleep(unsigned long usec) {
    long ts[2];
    ts[0] = usec / 1000000;
    ts[1] = (usec % 1000000) * 1000;
    return (int)sys_call2(101 /* SYS_nanosleep */, (long)ts, 0);
}

long time(long *tloc) {
    long ts[2];
    long r = sys_call2(113 /* SYS_clock_gettime */, 0 /* CLOCK_REALTIME */, (long)ts);
    if (r < 0) {
        long tv[2];
        sys_call2(169 /* SYS_gettimeofday */, (long)tv, 0);
        ts[0] = tv[0];
    }
    if (tloc) *tloc = ts[0];
    return ts[0];
}

struct tm {
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
static struct tm static_tm;

struct tm *gmtime(const long *timer) {
    long t = timer ? *timer : 0;
    static_tm.tm_sec = t % 60;
    static_tm.tm_min = (t / 60) % 60;
    static_tm.tm_hour = (t / 3600) % 24;
    static_tm.tm_mday = 1 + ((t / 86400) % 30);
    static_tm.tm_mon = ((t / (86400 * 30)) % 12);
    static_tm.tm_year = 70 + (t / (86400 * 365));
    static_tm.tm_wday = (4 + (t / 86400)) % 7;
    static_tm.tm_yday = (t / 86400) % 365;
    static_tm.tm_isdst = 0;
    return &static_tm;
}

struct tm *localtime(const long *timer) {
    return gmtime(timer);
}

#endif /* TARGET_RISCV32 */
