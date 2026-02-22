//
// BDOS syscall dispatcher.
// Called from the assembly entry stub (Syscall: label in cgb32p3.inc).
// Arguments arrive via B32CC calling convention: r4=num, r5=a1, r6=a2, r7=a3.
// Return value goes in r1.
//

#include "BDOS/bdos.h"

int bdos_syscall_dispatch(int num, int a1, int a2, int a3)
{
  switch (num)
  {
    case SYSCALL_PRINT_CHAR:
      term_putchar(a1);
      return 0;

    case SYSCALL_PRINT_STR:
      term_puts((char*)a1);
      return 0;

    case SYSCALL_READ_KEY:
      return bdos_keyboard_event_read();

    case SYSCALL_KEY_AVAILABLE:
      return bdos_keyboard_event_available();

    case SYSCALL_FS_OPEN:
      return brfs_open((char*)a1);

    case SYSCALL_FS_CLOSE:
      return brfs_close(a1);

    case SYSCALL_FS_READ:
      return brfs_read(a1, (unsigned int*)a2, (unsigned int)a3);

    case SYSCALL_FS_WRITE:
      return brfs_write(a1, (unsigned int*)a2, (unsigned int)a3);

    default:
      return -1;
  }
}
