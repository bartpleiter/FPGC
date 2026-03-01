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
    // ---- I/O ----
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

    // ---- Filesystem ----
    case SYSCALL_FS_OPEN:
      return brfs_open((char*)a1);

    case SYSCALL_FS_CLOSE:
      return brfs_close(a1);

    case SYSCALL_FS_READ:
      return brfs_read(a1, (unsigned int*)a2, (unsigned int)a3);

    case SYSCALL_FS_WRITE:
      return brfs_write(a1, (unsigned int*)a2, (unsigned int)a3);

    case SYSCALL_FS_SEEK:
      return brfs_seek(a1, (unsigned int)a2);

    case SYSCALL_FS_STAT:
      return brfs_stat((char*)a1, (struct brfs_dir_entry*)a2);

    case SYSCALL_FS_DELETE:
      return brfs_delete((char*)a1);

    case SYSCALL_FS_CREATE:
      return brfs_create_file((char*)a1);

    case SYSCALL_FS_FILESIZE:
      return brfs_file_size(a1);

    // ---- Shell integration ----
    case SYSCALL_SHELL_ARGC:
      return bdos_shell_prog_argc;

    case SYSCALL_SHELL_ARGV:
      return (int)bdos_shell_prog_argv;

    case SYSCALL_SHELL_GETCWD:
      return (int)bdos_shell_cwd;

    // ---- Terminal control ----
    case SYSCALL_TERM_PUT_CELL:
    {
      // a1=x, a2=y, a3=packed (tile<<8 | palette)
      unsigned int tile = ((unsigned int)a3 >> 8) & 0xFF;
      unsigned int palette = (unsigned int)a3 & 0xFF;
      term_put_cell((unsigned int)a1, (unsigned int)a2, tile, palette);
      return 0;
    }

    case SYSCALL_TERM_CLEAR:
      term_clear();
      return 0;

    case SYSCALL_TERM_SET_CURSOR:
      term_set_cursor((unsigned int)a1, (unsigned int)a2);
      return 0;

    case SYSCALL_TERM_GET_CURSOR:
    {
      // Return packed (x<<8 | y)
      unsigned int cx;
      unsigned int cy;
      term_get_cursor(&cx, &cy);
      return (int)((cx << 8) | cy);
    }

    // ---- Heap ----
    case SYSCALL_HEAP_ALLOC:
      return (int)bdos_heap_alloc((unsigned int)a1);

    // ---- Timing ----
    case SYSCALL_DELAY:
      delay((unsigned int)a1);
      return 0;

    // ---- GPU ----
    case SYSCALL_SET_PALETTE:
    {
      // a1 = palette index (0-31), a2 = palette value (bg<<8 | fg)
      unsigned int *palette_addr = (unsigned int *)(GPU_PALETTE_TABLE_ADDR + (unsigned int)a1);
      *palette_addr = (unsigned int)a2;
      return 0;
    }

    // ---- Process control ----
    case SYSCALL_EXIT:
    {
      // Terminate the calling user program immediately.
      // a1 = exit code.
      bdos_run_retval = a1;
      asm(
        "load32 0x7C00001 r1"   // IO_HW_STACK_PTR
        "load 13 r2"            // trampoline depth
        "write 0 r1 r2"         // discard everything above trampoline
        "jump Label_bdos_run_return"
      );
      return 0; // unreachable
    }

    // ---- Directory listing ----
    case SYSCALL_FS_READDIR:
      return brfs_read_dir((char*)a1, (struct brfs_dir_entry*)a2, (unsigned int)a3);

    default:
      return -1;
  }
}
