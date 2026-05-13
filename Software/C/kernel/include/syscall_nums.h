/*
 * syscall_nums.h — BDOS v4 syscall numbers.
 *
 * POSIX-aligned numbering. Clean break from v3.
 * All user programs must be recompiled for v4.
 */
#ifndef KERNEL_SYSCALL_NUMS_H
#define KERNEL_SYSCALL_NUMS_H

#define SYS_EXIT            0
#define SYS_READ            1
#define SYS_WRITE           2
#define SYS_OPEN            3
#define SYS_CLOSE           4
#define SYS_LSEEK           5
#define SYS_DUP2            6
#define SYS_PIPE            7
#define SYS_EXEC            8
#define SYS_YIELD           9
#define SYS_WAITPID        10
#define SYS_GETPID         11
#define SYS_GETCWD         12
#define SYS_CHDIR          13
#define SYS_MKDIR          14
#define SYS_UNLINK         15
#define SYS_READDIR        16
#define SYS_STAT           17
#define SYS_IOCTL          18
#define SYS_SBRK           19
#define SYS_SLEEP          20
#define SYS_GET_TIME_US    21
#define SYS_GET_KEY_STATE  22
#define SYS_NET_SEND       23
#define SYS_NET_RECV       24
#define SYS_ARGC           25
#define SYS_ARGV           26
#define SYS_FORMAT         27
#define SYS_SYNC           28
#define SYS_RENAME         29
#define SYS_TRUNCATE       30

/* Syscall dispatch function (called from crt0 Syscall entry) */
int syscall_dispatch(int num, int a1, int a2, int a3);

#endif /* KERNEL_SYSCALL_NUMS_H */
