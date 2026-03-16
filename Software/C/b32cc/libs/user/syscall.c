//
// User-side syscall library.
// Provides the low-level syscall() function and convenience wrappers.
//

#include "libs/user/syscall.h"

// Low-level syscall invocation.
// B32CC places arguments in r4-r7 (calling convention: r4=num, r5=a1, r6=a2, r7=a3).
// We jump to the syscall vector at absolute address 3.
// The kernel handler returns the result in r1.
int syscall(int num, int a1, int a2, int a3)
{
  int retval = 0;
  asm(
    "push r15                ; save caller's return address"
    "load 12 r11             ; r11 = 12 (syscall vector byte address)"
    "savpc r15               ; r15 = PC of this instruction"
    "add r15 12 r15          ; r15 = return point (after jumpr)"
    "jumpr 0 r11             ; jump to BDOS syscall handler"
    "write -4 r14 r1         ; store return value in retval"
    "pop r15                 ; restore caller's return address"
  );
  return retval;
}

// ---- Convenience wrappers ----

void sys_print_char(int ch)
{
  syscall(SYSCALL_PRINT_CHAR, ch, 0, 0);
}

void sys_print_str(char* s)
{
  syscall(SYSCALL_PRINT_STR, (int)s, 0, 0);
}

int sys_read_key()
{
  return syscall(SYSCALL_READ_KEY, 0, 0, 0);
}

int sys_key_available()
{
  return syscall(SYSCALL_KEY_AVAILABLE, 0, 0, 0);
}

int sys_fs_open(char* path)
{
  return syscall(SYSCALL_FS_OPEN, (int)path, 0, 0);
}

int sys_fs_close(int fd)
{
  return syscall(SYSCALL_FS_CLOSE, fd, 0, 0);
}

int sys_fs_read(int fd, unsigned int* buf, unsigned int count)
{
  return syscall(SYSCALL_FS_READ, fd, (int)buf, (int)count);
}

int sys_fs_write(int fd, unsigned int* buf, unsigned int count)
{
  return syscall(SYSCALL_FS_WRITE, fd, (int)buf, (int)count);
}

int sys_fs_seek(int fd, unsigned int offset)
{
  return syscall(SYSCALL_FS_SEEK, fd, (int)offset, 0);
}

int sys_fs_stat(char* path, unsigned int* entry_buf)
{
  return syscall(SYSCALL_FS_STAT, (int)path, (int)entry_buf, 0);
}

int sys_fs_delete(char* path)
{
  return syscall(SYSCALL_FS_DELETE, (int)path, 0, 0);
}

int sys_fs_create(char* path)
{
  return syscall(SYSCALL_FS_CREATE, (int)path, 0, 0);
}

int sys_fs_filesize(int fd)
{
  return syscall(SYSCALL_FS_FILESIZE, fd, 0, 0);
}

int sys_fs_readdir(char* path, unsigned int* entry_buf, unsigned int max_entries)
{
  return syscall(SYSCALL_FS_READDIR, (int)path, (int)entry_buf, (int)max_entries);
}

int sys_shell_argc()
{
  return syscall(SYSCALL_SHELL_ARGC, 0, 0, 0);
}

char** sys_shell_argv()
{
  return (char**)syscall(SYSCALL_SHELL_ARGV, 0, 0, 0);
}

char* sys_shell_getcwd()
{
  return (char*)syscall(SYSCALL_SHELL_GETCWD, 0, 0, 0);
}

void sys_term_put_cell(int x, int y, int tile_palette)
{
  syscall(SYSCALL_TERM_PUT_CELL, x, y, tile_palette);
}

void sys_term_clear()
{
  syscall(SYSCALL_TERM_CLEAR, 0, 0, 0);
}

void sys_term_set_cursor(int x, int y)
{
  syscall(SYSCALL_TERM_SET_CURSOR, x, y, 0);
}

int sys_term_get_cursor()
{
  return syscall(SYSCALL_TERM_GET_CURSOR, 0, 0, 0);
}

unsigned int* sys_heap_alloc(int size_words)
{
  return (unsigned int*)syscall(SYSCALL_HEAP_ALLOC, size_words, 0, 0);
}

void sys_delay(int ms)
{
  syscall(SYSCALL_DELAY, ms, 0, 0);
}

void sys_set_palette(int index, int value)
{
  syscall(SYSCALL_SET_PALETTE, index, value, 0);
}

void sys_set_pixel_palette(int index, int rgb24)
{
  syscall(SYSCALL_SET_PIXEL_PALETTE, index, rgb24, 0);
}

int sys_net_send(char *buf, int len)
{
  return syscall(SYSCALL_NET_SEND, (int)buf, len, 0);
}

int sys_net_recv(char *buf, int max_len)
{
  return syscall(SYSCALL_NET_RECV, (int)buf, max_len, 0);
}

int sys_net_packet_count()
{
  return syscall(SYSCALL_NET_PACKET_COUNT, 0, 0, 0);
}

void sys_net_get_mac(int *mac_buf)
{
  syscall(SYSCALL_NET_GET_MAC, (int)mac_buf, 0, 0);
}

void sys_exit(int code)
{
  syscall(SYSCALL_EXIT, code, 0, 0);
}

int sys_get_key_state()
{
  return syscall(SYSCALL_GET_KEY_STATE, 0, 0, 0);
}
