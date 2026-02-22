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
    "load 3 r11              ; r11 = 3 (syscall vector address)"
    "savpc r15               ; r15 = PC of this instruction"
    "add r15 3 r15           ; r15 = return point (after jumpr)"
    "jumpr 0 r11             ; jump to BDOS syscall handler"
    "write -1 r14 r1         ; store return value in retval"
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
