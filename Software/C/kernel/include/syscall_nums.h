/*
 * syscall_nums.h — BDOS v4 syscall numbers (POSIX-aligned).
 *
 * Clean v4 design — no backward compatibility with v3 numbering.
 * All user programs are compiled against the v4 userlib.
 */
#ifndef KERNEL_SYSCALL_NUMS_H
#define KERNEL_SYSCALL_NUMS_H

/* ---- Core process control ---- */
#define SYS_EXIT             1
#define SYS_YIELD            2
#define SYS_SPAWN            3
#define SYS_WAITPID          4
#define SYS_GETPID           5
#define SYS_KILL             6

/* ---- File I/O ---- */
#define SYS_OPEN            10
#define SYS_CLOSE           11
#define SYS_READ            12
#define SYS_WRITE           13
#define SYS_LSEEK           14
#define SYS_DUP2            15

/* ---- Filesystem ---- */
#define SYS_UNLINK          20
#define SYS_MKDIR           21
#define SYS_READDIR         22
#define SYS_RENAME          23
#define SYS_STAT            24
#define SYS_SYNC            25
#define SYS_TRUNCATE        26
#define SYS_FORMAT          27
#define SYS_SD_FORMAT       28

/* ---- Process environment ---- */
#define SYS_CHDIR           30
#define SYS_GETCWD          31
#define SYS_ARGC            32
#define SYS_ARGV            33
#define SYS_SBRK            34

/* ---- Timing / input ---- */
#define SYS_SLEEP           40
#define SYS_GET_KEY_STATE   41
#define SYS_GET_TIME_US     42

/* ---- Networking ---- */
#define SYS_NET_SEND        50
#define SYS_NET_RECV        51
#define SYS_NET_PACKET_COUNT 52
#define SYS_NET_GET_MAC     53

/* ---- IPC ---- */
#define SYS_PIPE            60
#define SYS_IOCTL           61

/* Syscall dispatch function (called from crt0 Syscall entry) */
int syscall_dispatch(int num, int a1, int a2, int a3);

#endif /* KERNEL_SYSCALL_NUMS_H */
