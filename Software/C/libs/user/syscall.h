#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers, must match BDOS bdos.h definitions
#define SYSCALL_PRINT_CHAR     0
#define SYSCALL_PRINT_STR      1
#define SYSCALL_READ_KEY       2
#define SYSCALL_KEY_AVAILABLE  3
#define SYSCALL_FS_OPEN        4
#define SYSCALL_FS_CLOSE       5
#define SYSCALL_FS_READ        6
#define SYSCALL_FS_WRITE       7

// Low-level syscall invocation
int syscall(int num, int a1, int a2, int a3);

// Convenience wrappers
void sys_print_char(int ch);
void sys_print_str(char* s);
int sys_read_key();
int sys_key_available();
int sys_fs_open(char* path);
int sys_fs_close(int fd);
int sys_fs_read(int fd, unsigned int* buf, unsigned int count);
int sys_fs_write(int fd, unsigned int* buf, unsigned int count);

#endif // SYSCALL_H
