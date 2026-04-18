#ifndef SYSCALL_H
#define SYSCALL_H

/*
 * Userland syscall interface — must stay in sync with BDOS bdos_syscall.h.
 *
 * shell-terminal-v2 Phase E removed several legacy syscalls. Programs
 * that still call the retired wrappers (sys_term_put_cell, sys_read_key,
 * sys_set_palette, sys_uart_print_str, ...) will not link. See the
 * MIGRATION block below for replacements; snake.c was ported as a
 * reference example.
 */

/*
 * MIGRATION (legacy -> replacement):
 *
 *   sys_putc(c) / sys_putstr(s)            -> already wrapped on sys_write(1, ...)
 *   sys_read_key() / sys_key_available()   -> open("/dev/tty", O_RDONLY|O_RAW
 *                                              [|O_NONBLOCK]) then
 *                                              sys_read(fd, &event, 4) — event
 *                                              is little-endian uint32 of the
 *                                              key code (0x00..0xFF for ASCII,
 *                                              0x100+ for KEY_UP, KEY_F1, ...).
 *   sys_term_put_cell(x, y, ch|pal)        -> sys_write(1, "\x1b[<y+1>;<x+1>H"
 *                                              "\x1b[<pal>m<ch>", n)
 *   sys_term_clear()                       -> sys_write(1, "\x1b[2J\x1b[H", 7)
 *   sys_term_set_cursor(x, y)              -> sys_write(1, "\x1b[<y+1>;<x+1>H", n)
 *   sys_term_get_cursor()                  -> not exposed; track locally
 *   sys_set_palette(i, v)                  -> no replacement yet (was raw MMIO)
 *   sys_set_pixel_palette(i, rgb)          -> no replacement yet (was raw MMIO)
 *   sys_uart_print_str(s)                  -> sys_write(2, s, n) (stderr)
 */

/* Syscall numbers — kept in sync with BDOS bdos_syscall.h */
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
#define SYSCALL_HEAP_ALLOC       20
#define SYSCALL_DELAY            21
#define SYSCALL_EXIT             23
#define SYSCALL_FS_READDIR       24
#define SYSCALL_GET_KEY_STATE    25
#define SYSCALL_NET_SEND         27
#define SYSCALL_NET_RECV         28
#define SYSCALL_NET_PACKET_COUNT 29
#define SYSCALL_NET_GET_MAC      30
#define SYSCALL_FS_MKDIR         33
#define SYSCALL_OPEN             34
#define SYSCALL_READ             35
#define SYSCALL_WRITE            36
#define SYSCALL_CLOSE            37
#define SYSCALL_LSEEK            38
#define SYSCALL_DUP2             39
#define SYSCALL_FS_FORMAT        40

/* Flags for sys_open() (must match BDOS_O_* in bdos_vfs.h) */
#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      0x03
#define O_APPEND    0x04
#define O_CREAT     0x08
#define O_TRUNC     0x10
#define O_RAW       0x20    /* /dev/tty: deliver 4-byte event packets */
#define O_NONBLOCK  0x40    /* /dev/tty raw: don't block when FIFO empty */

/* whence values for sys_lseek() */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* Standard fds (pre-opened by BDOS to /dev/tty) */
#define STDIN_FD   0
#define STDOUT_FD  1
#define STDERR_FD  2

/* Key state bitmap bit positions (matching BDOS bdos_hid.h) */
#define KEYSTATE_W        0x0001
#define KEYSTATE_A        0x0002
#define KEYSTATE_S        0x0004
#define KEYSTATE_D        0x0008
#define KEYSTATE_UP       0x0010
#define KEYSTATE_DOWN     0x0020
#define KEYSTATE_LEFT     0x0040
#define KEYSTATE_RIGHT    0x0080
#define KEYSTATE_SPACE    0x0100
#define KEYSTATE_SHIFT    0x0200
#define KEYSTATE_CTRL     0x0400
#define KEYSTATE_ESCAPE   0x0800
#define KEYSTATE_E        0x1000
#define KEYSTATE_Q        0x2000

/* Special key codes (must match BDOS_KEY_* in bdos_hid.h).
   Returned in the low 32 bits of an O_RAW /dev/tty event packet. */
#define KEY_SPECIAL_BASE 0x100
#define KEY_UP           (KEY_SPECIAL_BASE + 1)
#define KEY_DOWN         (KEY_SPECIAL_BASE + 2)
#define KEY_LEFT         (KEY_SPECIAL_BASE + 3)
#define KEY_RIGHT        (KEY_SPECIAL_BASE + 4)
#define KEY_INSERT       (KEY_SPECIAL_BASE + 5)
#define KEY_DELETE       (KEY_SPECIAL_BASE + 6)
#define KEY_HOME         (KEY_SPECIAL_BASE + 7)
#define KEY_END          (KEY_SPECIAL_BASE + 8)
#define KEY_PAGEUP       (KEY_SPECIAL_BASE + 9)
#define KEY_PAGEDOWN     (KEY_SPECIAL_BASE + 10)
#define KEY_F1           (KEY_SPECIAL_BASE + 11)
#define KEY_F2           (KEY_SPECIAL_BASE + 12)
#define KEY_F3           (KEY_SPECIAL_BASE + 13)
#define KEY_F4           (KEY_SPECIAL_BASE + 14)
#define KEY_F5           (KEY_SPECIAL_BASE + 15)
#define KEY_F6           (KEY_SPECIAL_BASE + 16)
#define KEY_F7           (KEY_SPECIAL_BASE + 17)
#define KEY_F8           (KEY_SPECIAL_BASE + 18)
#define KEY_F9           (KEY_SPECIAL_BASE + 19)
#define KEY_F10          (KEY_SPECIAL_BASE + 20)
#define KEY_F11          (KEY_SPECIAL_BASE + 21)
#define KEY_F12          (KEY_SPECIAL_BASE + 22)

/* Low-level syscall invocation (implemented in syscall_asm.asm) */
int syscall(int num, int a1, int a2, int a3);

/* ---- Convenience wrappers: I/O ---- */
void sys_putc  (int ch);            /* fd 1, honours redirection */
void sys_putstr(const char *s);     /* fd 1, honours redirection */

/* ---- Convenience wrappers: Filesystem ---- */
int  sys_fs_open    (const char *path);
int  sys_fs_close   (int fd);
int  sys_fs_read    (int fd, void *buf, int count);
int  sys_fs_write   (int fd, void *buf, int count);
int  sys_fs_seek    (int fd, int offset);
int  sys_fs_stat    (const char *path, void *entry_buf);
int  sys_fs_delete  (const char *path);
int  sys_fs_create  (const char *path);
int  sys_fs_filesize(int fd);
int  sys_fs_readdir (const char *path, void *entry_buf, int max_entries);
int  sys_fs_mkdir   (const char *path);
int  sys_fs_format  (int blocks, int words_per_block, char *label);

/* ---- Convenience wrappers: Shell ---- */
int    sys_shell_argc  (void);
char **sys_shell_argv  (void);
char  *sys_shell_getcwd(void);

/* ---- Convenience wrappers: Heap ---- */
void *sys_heap_alloc(int size);

/* ---- Convenience wrappers: Timing ---- */
void sys_delay(int ms);

/* ---- Convenience wrappers: Networking (raw Ethernet) ---- */
int  sys_net_send        (char *buf, int len);
int  sys_net_recv        (char *buf, int max_len);
int  sys_net_packet_count(void);
void sys_net_get_mac     (int *mac_buf);

/* ---- Convenience wrappers: VFS / fd-oriented byte I/O ---- */
int  sys_open (const char *path, int flags);
int  sys_close(int fd);
int  sys_read (int fd, void *buf, int len);
int  sys_write(int fd, const void *buf, int len);
int  sys_lseek(int fd, int offset, int whence);
int  sys_dup2 (int oldfd, int newfd);

/* ---- Convenience wrappers: Process control ---- */
void sys_exit(int code);

/* ---- Convenience wrappers: Key state ---- */
int sys_get_key_state(void);

/* ---- Convenience wrappers: TTY events (replacement for sys_read_key) ----
 *
 * sys_tty_open_raw() opens /dev/tty in raw mode. The returned fd reads
 * 4-byte little-endian event packets via sys_read(fd, buf, 4*N).
 * sys_tty_event_read(fd, blocking) is a thin helper that does that
 * read and decodes one packet. Returns the event code (>=0), or -1 if
 * non-blocking and no event was queued.
 */
int sys_tty_open_raw(int nonblocking);
int sys_tty_event_read(int fd, int blocking);

#endif /* SYSCALL_H */
