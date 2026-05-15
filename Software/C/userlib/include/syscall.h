#ifndef SYSCALL_H
#define SYSCALL_H

/*
 * BDOS v4 userland syscall interface.
 *
 * POSIX-aligned syscall numbers. Must stay in sync with
 * kernel/include/syscall_nums.h.
 */

/* ---- Syscall numbers ---- */

/* Core process control (1-5) */
#define SYS_EXIT             1
#define SYS_YIELD            2
#define SYS_EXEC             3
#define SYS_WAITPID          4
#define SYS_GETPID           5
#define SYS_KILL             6

/* File I/O (10-15) */
#define SYS_OPEN            10
#define SYS_CLOSE           11
#define SYS_READ            12
#define SYS_WRITE           13
#define SYS_LSEEK           14
#define SYS_DUP2            15

/* Filesystem (20-28) */
#define SYS_UNLINK          20
#define SYS_MKDIR           21
#define SYS_READDIR         22
#define SYS_RENAME          23
#define SYS_STAT            24
#define SYS_SYNC            25
#define SYS_TRUNCATE        26
#define SYS_FORMAT          27
#define SYS_SD_FORMAT       28

/* Process environment (30-34) */
#define SYS_CHDIR           30
#define SYS_GETCWD          31
#define SYS_ARGC            32
#define SYS_ARGV            33
#define SYS_SBRK            34

/* Timing / input (40-42) */
#define SYS_SLEEP           40
#define SYS_GET_KEY_STATE   41
#define SYS_GET_TIME_US     42

/* Networking (50-53) */
#define SYS_NET_SEND        50
#define SYS_NET_RECV        51
#define SYS_NET_PACKET_COUNT 52
#define SYS_NET_GET_MAC     53

/* IPC (60-61) */
#define SYS_PIPE            60
#define SYS_IOCTL           61

/* ---- Flags for sys_open() (must match kernel vfs.h) ---- */
#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      0x03
#define O_APPEND    0x04
#define O_CREAT     0x08
#define O_TRUNC     0x10
#define O_RAW       0x20    /* /dev/tty: deliver 4-byte event packets */
#define O_NONBLOCK  0x40    /* /dev/tty raw: don't block when FIFO empty */

/* ---- whence values for sys_lseek() ---- */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* ---- Standard fds (pre-opened by BDOS to /dev/tty) ---- */
#define STDIN_FD   0
#define STDOUT_FD  1
#define STDERR_FD  2

/* ---- Key state bitmap ---- */
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

/* ---- Special key codes for O_RAW /dev/tty events ---- */
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

/* ---- Low-level syscall invocation (implemented in syscall_asm.asm) ---- */
int syscall(int num, int a1, int a2, int a3);

/* ---- Process control ---- */
void sys_exit(int code);
int  sys_getpid(void);
int  sys_kill(int pid);
void sys_yield(void);
int  sys_exec(const char *path, int argc, const char **argv);
int  sys_waitpid(int pid);
int  sys_stat(const char *path, void *buf);
int  sys_ioctl(int fd, int cmd, int arg);

/* ---- I/O convenience ---- */
void sys_putc  (int ch);
void sys_putstr(const char *s);

/* ---- File I/O ---- */
int  sys_open (const char *path, int flags);
int  sys_close(int fd);
int  sys_read (int fd, void *buf, int len);
int  sys_write(int fd, const void *buf, int len);
int  sys_lseek(int fd, int offset, int whence);
int  sys_dup2 (int oldfd, int newfd);

/* ---- Filesystem ---- */
int  sys_unlink(const char *path);
int  sys_mkdir(const char *path);
int  sys_readdir(const char *path, void *entry_buf, int max_entries);
int  sys_rename(const char *oldpath, const char *newpath);
int  sys_sync(void);
int  sys_fs_format(int blocks, int words_per_block, char *label);
int  sys_sd_format(int blocks, int words_per_block, char *label);

/* ---- Process environment ---- */
int    sys_argc  (void);
char **sys_argv  (void);
int    sys_getcwd(char *buf, int size);
int    sys_chdir (const char *path);
void  *sys_sbrk  (int incr);

/* ---- Timing ---- */
void sys_sleep(int ms);
int  sys_get_time_us(void);

/* ---- Input ---- */
int sys_get_key_state(void);

/* ---- Networking ---- */
int  sys_net_send        (char *buf, int len);
int  sys_net_recv        (char *buf, int max_len);
int  sys_net_packet_count(void);
void sys_net_get_mac     (int *mac_buf);

/* ---- TTY event helpers ---- */
int sys_tty_open_raw(int nonblocking);
int sys_tty_event_read(int fd, int blocking);

#endif /* SYSCALL_H */
