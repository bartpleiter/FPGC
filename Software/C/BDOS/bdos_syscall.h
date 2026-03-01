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
#define SYSCALL_FS_SEEK        8
#define SYSCALL_FS_STAT        9
#define SYSCALL_FS_DELETE      10
#define SYSCALL_FS_CREATE      11
#define SYSCALL_FS_FILESIZE    12
#define SYSCALL_SHELL_ARGC     13
#define SYSCALL_SHELL_ARGV     14
#define SYSCALL_SHELL_GETCWD   15
#define SYSCALL_TERM_PUT_CELL  16
#define SYSCALL_TERM_CLEAR     17
#define SYSCALL_TERM_SET_CURSOR 18
#define SYSCALL_TERM_GET_CURSOR 19
#define SYSCALL_HEAP_ALLOC     20
#define SYSCALL_DELAY          21
#define SYSCALL_SET_PALETTE    22
#define SYSCALL_EXIT           23
#define SYSCALL_FS_READDIR     24
#define SYSCALL_GET_KEY_STATE  25

int bdos_syscall_dispatch(int num, int a1, int a2, int a3);

#endif // BDOS_SYSCALL_H
