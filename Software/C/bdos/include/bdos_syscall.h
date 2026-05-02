#ifndef BDOS_SYSCALL_H
#define BDOS_SYSCALL_H

/*
 * BDOS syscall numbers.
 *
 * Numbers 0..3, 16..19, 22, 26, 31, 32 were the legacy direct-to-hardware
 * codes from BDOS v3 (raw print, raw read, tile-level terminal access,
 * direct palette MMIO, raw UART). They were retired in shell-terminal-v2
 * Phase E in favour of the byte-oriented fd API + ANSI escapes routed
 * through write(1, ...). The slots are kept reserved (commented out) so
 * existing call sites get a -1 return from the dispatcher and can be
 * found / migrated gradually.
 *
 * Replacements:
 *   PRINT_CHAR / PRINT_STR        -> write(1, ...)
 *   READ_KEY / KEY_AVAILABLE      -> open("/dev/tty", O_RDONLY|O_RAW) then
 *                                    read 4-byte little-endian event packets
 *   TERM_PUT_CELL / SET_CURSOR /  -> ANSI escapes via write(1, ...)
 *      CLEAR / GET_CURSOR
 *   SET_PALETTE / SET_PIXEL_PALETTE -> no fd-API substitute; will be revisited
 *                                    if a new userland program needs them
 *   UART_PRINT_CHAR / UART_PRINT_STR -> write to fd 2 (stderr) or open /dev/uart
 *
 * The fd-oriented API (SYSCALL_OPEN, _READ, _WRITE, _CLOSE, _LSEEK, _DUP2)
 * is the supported way for user programs to talk to the world.
 */

/* 0 reserved (was SYSCALL_PRINT_CHAR)  -- use write(1, &c, 1) */
/* 1 reserved (was SYSCALL_PRINT_STR)   -- use write(1, s, n)  */
/* 2 reserved (was SYSCALL_READ_KEY)    -- open /dev/tty O_RAW */
/* 3 reserved (was SYSCALL_KEY_AVAILABLE) -- non-blocking read on /dev/tty O_RAW */
#define SYSCALL_FS_OPEN          4
#define SYSCALL_FS_CLOSE         5
#define SYSCALL_FS_READ          6
#define SYSCALL_FS_WRITE         7
#define SYSCALL_FS_SEEK          8
#define SYSCALL_FS_STAT          9
#define SYSCALL_FS_DELETE        10
#define SYSCALL_FS_CREATE        11
#define SYSCALL_FS_FILESIZE      12
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
#define SYSCALL_FS_READDIR       24
#define SYSCALL_GET_KEY_STATE    25
/* 26 reserved (was SYSCALL_SET_PIXEL_PALETTE) -- TODO: pick replacement */
#define SYSCALL_NET_SEND         27
#define SYSCALL_NET_RECV         28
#define SYSCALL_NET_PACKET_COUNT 29
#define SYSCALL_NET_GET_MAC      30
/* 31 reserved (was SYSCALL_UART_PRINT_CHAR) -- use stderr (fd 2) */
/* 32 reserved (was SYSCALL_UART_PRINT_STR)  -- use stderr (fd 2) */
#define SYSCALL_FS_MKDIR         33
#define SYSCALL_OPEN             34
#define SYSCALL_READ             35
#define SYSCALL_WRITE            36
#define SYSCALL_CLOSE            37
#define SYSCALL_LSEEK            38
#define SYSCALL_DUP2             39
#define SYSCALL_FS_FORMAT        40   /* args: blocks, words_per_block, label_ptr */
#define SYSCALL_SD_FORMAT        41   /* args: blocks, words_per_block, label_ptr (SD card) */

/* Syscall dispatch function */
int bdos_syscall_dispatch(int num, int a1, int a2, int a3);

#endif /* BDOS_SYSCALL_H */
#ifndef BDOS_SYSCALL_H
#define BDOS_SYSCALL_H

/* Syscall numbers */
/* 0 and 1 reserved (formerly SYSCALL_PRINT_CHAR / SYSCALL_PRINT_STR;
 * userland now goes through SYSCALL_WRITE on fd 1). */
#define SYSCALL_READ_KEY         2
#define SYSCALL_KEY_AVAILABLE    3
#define SYSCALL_FS_OPEN          4
#define SYSCALL_FS_CLOSE         5
#define SYSCALL_FS_READ          6
#define SYSCALL_FS_WRITE         7
#define SYSCALL_FS_SEEK          8
#define SYSCALL_FS_STAT          9
#define SYSCALL_FS_DELETE        10
#define SYSCALL_FS_CREATE        11
#define SYSCALL_FS_FILESIZE      12
#define SYSCALL_SHELL_ARGC       13
#define SYSCALL_SHELL_ARGV       14
#define SYSCALL_SHELL_GETCWD     15
#define SYSCALL_TERM_PUT_CELL    16
#define SYSCALL_TERM_CLEAR       17
#define SYSCALL_TERM_SET_CURSOR  18
#define SYSCALL_TERM_GET_CURSOR  19
#define SYSCALL_HEAP_ALLOC       20
#define SYSCALL_DELAY            21
#define SYSCALL_SET_PALETTE      22
#define SYSCALL_EXIT             23
#define SYSCALL_FS_READDIR       24
#define SYSCALL_GET_KEY_STATE    25
#define SYSCALL_SET_PIXEL_PALETTE 26
#define SYSCALL_NET_SEND         27
#define SYSCALL_NET_RECV         28
#define SYSCALL_NET_PACKET_COUNT 29
#define SYSCALL_NET_GET_MAC      30
#define SYSCALL_UART_PRINT_CHAR  31
#define SYSCALL_UART_PRINT_STR   32
#define SYSCALL_FS_MKDIR         33
#define SYSCALL_OPEN             34
#define SYSCALL_READ             35
#define SYSCALL_WRITE            36
#define SYSCALL_CLOSE            37
#define SYSCALL_LSEEK            38
#define SYSCALL_DUP2             39

/* Syscall dispatch function */
int bdos_syscall_dispatch(int num, int a1, int a2, int a3);

#endif /* BDOS_SYSCALL_H */
