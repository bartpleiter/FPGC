#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers, must match BDOS bdos_syscall.h definitions
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

// Special key codes (must match BDOS_KEY_* in bdos_hid.h)
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

// Low-level syscall invocation
int syscall(int num, int a1, int a2, int a3);

// Convenience wrappers — I/O
void sys_print_char(int ch);
void sys_print_str(char* s);
int sys_read_key();
int sys_key_available();

// Convenience wrappers — Filesystem
int sys_fs_open(char* path);
int sys_fs_close(int fd);
int sys_fs_read(int fd, unsigned int* buf, unsigned int count);
int sys_fs_write(int fd, unsigned int* buf, unsigned int count);
int sys_fs_seek(int fd, unsigned int offset);
int sys_fs_stat(char* path, unsigned int* entry_buf);
int sys_fs_delete(char* path);
int sys_fs_create(char* path);
int sys_fs_filesize(int fd);

// Convenience wrappers — Shell
int sys_shell_argc();
char** sys_shell_argv();
char* sys_shell_getcwd();

// Convenience wrappers — Terminal
void sys_term_put_cell(int x, int y, int tile_palette);
void sys_term_clear();
void sys_term_set_cursor(int x, int y);
int sys_term_get_cursor();

// Convenience wrappers — Heap
unsigned int* sys_heap_alloc(int size_words);

#endif // SYSCALL_H
