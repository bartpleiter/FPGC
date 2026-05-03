#ifndef BDOS_SYSCALL_H
#define BDOS_SYSCALL_H

/*
 * BDOS syscall numbers.
 *
 * Numbers 0..3, 16..19, 22, 26, 31, 32 were the legacy direct-to-hardware
 * codes from BDOS v3 (raw print, raw read, tile-level terminal access,
 * direct palette MMIO, raw UART). They were retired in shell-terminal-v2
 * Phase E in favour of the byte-oriented fd API + ANSI escapes routed
 * through write(1, ...).
 *
 * Numbers 4..12, 24, 33 were the raw-BRFS filesystem syscalls (FS_OPEN,
 * FS_CLOSE, FS_READ, FS_WRITE, FS_SEEK, FS_STAT, FS_DELETE, FS_CREATE,
 * FS_FILESIZE, FS_READDIR, FS_MKDIR). These bypassed VFS and went
 * directly to brfs_spi. All userland callers have been migrated to the
 * VFS API (SYSCALL_OPEN, _READ, _WRITE, _CLOSE, _LSEEK, _UNLINK,
 * _MKDIR, _READDIR). Slots reserved — dispatcher returns -1.
 *
 * The VFS API (SYSCALL_OPEN, _READ, _WRITE, _CLOSE, _LSEEK, _DUP2,
 * _UNLINK, _MKDIR, _READDIR) is the supported way for user programs
 * to perform file I/O. Path routing: /dev/* → devices, /sdcard/* →
 * SD card BRFS, else → SPI flash BRFS.
 */

/* 0 reserved (was SYSCALL_PRINT_CHAR)  -- use write(1, &c, 1) */
/* 1 reserved (was SYSCALL_PRINT_STR)   -- use write(1, s, n)  */
/* 2 reserved (was SYSCALL_READ_KEY)    -- open /dev/tty O_RAW */
/* 3 reserved (was SYSCALL_KEY_AVAILABLE) -- non-blocking read on /dev/tty O_RAW */
/* 4 reserved (was SYSCALL_FS_OPEN)     -- use sys_open() */
/* 5 reserved (was SYSCALL_FS_CLOSE)    -- use sys_close() */
/* 6 reserved (was SYSCALL_FS_READ)     -- use sys_read() */
/* 7 reserved (was SYSCALL_FS_WRITE)    -- use sys_write() */
/* 8 reserved (was SYSCALL_FS_SEEK)     -- use sys_lseek() */
/* 9 reserved (was SYSCALL_FS_STAT)     -- no VFS replacement yet */
/* 10 reserved (was SYSCALL_FS_DELETE)  -- use sys_unlink() */
/* 11 reserved (was SYSCALL_FS_CREATE)  -- use sys_open(O_CREAT) */
/* 12 reserved (was SYSCALL_FS_FILESIZE) -- use sys_lseek(SEEK_END) */
#define SYSCALL_SHELL_ARGC       13
#define SYSCALL_SHELL_ARGV       14
#define SYSCALL_SHELL_GETCWD     15
/* 16 reserved (was SYSCALL_TERM_PUT_CELL)   -- ANSI cursor + SGR + char */
/* 17 reserved (was SYSCALL_TERM_CLEAR)      -- write "\x1b[2J\x1b[H" */
/* 18 reserved (was SYSCALL_TERM_SET_CURSOR) -- write "\x1b[r;cH"     */
/* 19 reserved (was SYSCALL_TERM_GET_CURSOR) -- no replacement needed */
#define SYSCALL_HEAP_ALLOC       20
#define SYSCALL_DELAY            21
/* 22 reserved (was SYSCALL_SET_PALETTE)     -- TODO: pick replacement */
#define SYSCALL_EXIT             23
/* 24 reserved (was SYSCALL_FS_READDIR) -- use sys_readdir() */
#define SYSCALL_GET_KEY_STATE    25
/* 26 reserved (was SYSCALL_SET_PIXEL_PALETTE) -- TODO: pick replacement */
#define SYSCALL_NET_SEND         27
#define SYSCALL_NET_RECV         28
#define SYSCALL_NET_PACKET_COUNT 29
#define SYSCALL_NET_GET_MAC      30
/* 31 reserved (was SYSCALL_UART_PRINT_CHAR) -- use stderr (fd 2) */
/* 32 reserved (was SYSCALL_UART_PRINT_STR)  -- use stderr (fd 2) */
/* 33 reserved (was SYSCALL_FS_MKDIR)   -- use sys_mkdir() */
#define SYSCALL_OPEN             34
#define SYSCALL_READ             35
#define SYSCALL_WRITE            36
#define SYSCALL_CLOSE            37
#define SYSCALL_LSEEK            38
#define SYSCALL_DUP2             39
#define SYSCALL_FS_FORMAT        40   /* args: blocks, words_per_block, label_ptr */
#define SYSCALL_SD_FORMAT        41   /* args: blocks, words_per_block, label_ptr (SD card) */
#define SYSCALL_UNLINK           42   /* VFS delete: routes through path prefix */
#define SYSCALL_MKDIR            43   /* VFS mkdir: routes through path prefix */
#define SYSCALL_READDIR          44   /* VFS readdir: routes through path prefix */

/* Syscall dispatch function */
int bdos_syscall_dispatch(int num, int a1, int a2, int a3);

#endif /* BDOS_SYSCALL_H */
