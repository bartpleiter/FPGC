#ifndef BDOS_SYSCALL_H
#define BDOS_SYSCALL_H

// Syscall numbers
#define SYSCALL_PRINT_CHAR     0
#define SYSCALL_PRINT_STR      1
#define SYSCALL_READ_KEY       2
#define SYSCALL_KEY_AVAILABLE  3
#define SYSCALL_FS_OPEN        4
#define SYSCALL_FS_CLOSE       5
#define SYSCALL_FS_READ        6
#define SYSCALL_FS_WRITE       7

int bdos_syscall_dispatch(int num, int a1, int a2, int a3);

#endif // BDOS_SYSCALL_H
