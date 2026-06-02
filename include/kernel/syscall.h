#ifndef SYSCALL_H
#define SYSCALL_H

#define SYS_EXIT    1
#define SYS_PUTCHAR 2
#define SYS_PRINT   3
#define SYS_CLEAR   4
#define SYS_SETCURSOR 5
#define SYS_GETKEY  6
#define SYS_OPEN    7
#define SYS_READ    8
#define SYS_WRITE   9
#define SYS_CLOSE   10

static inline void sys_exit() {
    __asm__ volatile(
        "movl $1, %%eax\n"
        "int $0x80\n"
        :
        :
        : "eax", "memory"
    );
}

static inline void sys_putchar(unsigned int c) {
    __asm__ volatile(
        "movl $2, %%eax\n"
        "movl %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"(c)
        : "eax", "ebx", "memory"
    );
}

static inline void sys_print(const char* str) {
    __asm__ volatile(
        "movl $3, %%eax\n"
        "movl %0, %%ebx\n"
        "int $0x80\n"
        :
        : "r"(str)
        : "eax", "ebx", "memory"
    );
}

static inline void sys_clear() {
    __asm__ volatile(
        "movl $4, %%eax\n"
        "int $0x80\n"
        :
        :
        : "eax", "ebx", "ecx", "edx", "esi", "edi", "memory"
    );
}

static inline void sys_setcursor(int row, int col) {
    __asm__ volatile(
        "movl $5, %%eax\n"
        "movl %0, %%ebx\n"
        "movl %1, %%ecx\n"
        "int $0x80\n"
        : : "r"(row), "r"(col) : "eax", "ebx", "ecx", "memory"
    );
}

static inline unsigned char sys_getkey() {
    unsigned char result;
    __asm__ volatile(
        "movl $6, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        :
        : "memory"
    );
    return result;
}

static inline int sys_open(const char* filename) {
    int result;
    __asm__ volatile(
        "movl $7, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(filename)
        : "memory"
    );
    return result;
}

static inline int sys_read(int fd, void* buffer, unsigned int size) {
    int result;
    __asm__ volatile(
        "movl $8, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
    return result;
}

static inline int sys_write(int fd, const void* buffer, unsigned int size) {
    int result;
    __asm__ volatile(
        "movl $9, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
    return result;
}

static inline int sys_close(int fd) {
    int result;
    __asm__ volatile(
        "movl $10, %%eax\n"
        "movl %1, %%ebx\n"
        "int $0x80\n"
        : "=a"(result)
        : "r"(fd)
        : "ebx", "memory"
    );
    return result;
}

#endif