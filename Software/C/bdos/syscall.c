#include "bdos.h"

/* Assembly helper for EXIT syscall (from slot_asm.asm) */
extern void bdos_syscall_exit(int exit_code);

int bdos_syscall_dispatch(int num, int a1, int a2, int a3)
{
  switch (num)
  {
    /* ---- I/O ---- */
    case SYSCALL_PRINT_CHAR:
      term_putchar(a1);
      return 0;

    case SYSCALL_PRINT_STR:
      term_puts((char *)a1);
      return 0;

    case SYSCALL_READ_KEY:
      return bdos_keyboard_event_read();

    case SYSCALL_KEY_AVAILABLE:
      return bdos_keyboard_event_available();

    /* ---- Filesystem ---- */
    case SYSCALL_FS_OPEN:
      return brfs_open((char *)a1);

    case SYSCALL_FS_CLOSE:
      return brfs_close(a1);

    case SYSCALL_FS_READ:
      return brfs_read(a1, (unsigned int *)a2, (unsigned int)a3);

    case SYSCALL_FS_WRITE:
      return brfs_write(a1, (unsigned int *)a2, (unsigned int)a3);

    case SYSCALL_FS_SEEK:
      return brfs_seek(a1, (unsigned int)a2);

    case SYSCALL_FS_STAT:
      return brfs_stat((char *)a1, (struct brfs_dir_entry *)a2);

    case SYSCALL_FS_DELETE:
      return brfs_delete((char *)a1);

    case SYSCALL_FS_CREATE:
      return brfs_create_file((char *)a1);

    case SYSCALL_FS_FILESIZE:
      return brfs_file_size(a1);

    /* ---- Shell integration ---- */
    case SYSCALL_SHELL_ARGC:
      return bdos_shell_prog_argc;

    case SYSCALL_SHELL_ARGV:
      return (int)bdos_shell_prog_argv;

    case SYSCALL_SHELL_GETCWD:
      return (int)bdos_shell_cwd;

    /* ---- Terminal control ---- */
    case SYSCALL_TERM_PUT_CELL:
    {
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
      unsigned int cx;
      unsigned int cy;
      term_get_cursor(&cx, &cy);
      return (int)((cx << 8) | cy);
    }

    /* ---- Heap ---- */
    case SYSCALL_HEAP_ALLOC:
      return (int)bdos_heap_alloc((unsigned int)a1);

    /* ---- Timing ---- */
    case SYSCALL_DELAY:
      delay((unsigned int)a1);
      return 0;

    /* ---- GPU ---- */
    case SYSCALL_SET_PALETTE:
    {
      unsigned int *palette_addr = (unsigned int *)(FPGC_GPU_PALETTE_TABLE + (unsigned int)a1 * sizeof(unsigned int));
      *palette_addr = (unsigned int)a2;
      return 0;
    }

    /* ---- Process control ---- */
    case SYSCALL_EXIT:
      bdos_syscall_exit(a1);
      return 0; /* unreachable */

    /* ---- Directory listing ---- */
    case SYSCALL_FS_READDIR:
      return brfs_read_dir((char *)a1, (struct brfs_dir_entry *)a2, (unsigned int)a3);

    /* ---- Key state ---- */
    case SYSCALL_GET_KEY_STATE:
      return (int)bdos_key_state_bitmap;

    /* ---- Pixel Palette ---- */
    case SYSCALL_SET_PIXEL_PALETTE:
    {
      unsigned int *pixel_palette_addr = (unsigned int *)(FPGC_GPU_PIXEL_PALETTE + (unsigned int)a1 * sizeof(unsigned int));
      *pixel_palette_addr = (unsigned int)a2;
      return 0;
    }

    /* ---- Networking (raw Ethernet) ---- */
    case SYSCALL_NET_SEND:
      fnp_net_user_owned = 1;
      return enc28j60_packet_send((char *)a1, a2);

    case SYSCALL_NET_RECV:
      fnp_net_user_owned = 1;
      return bdos_net_ringbuf_pop((char *)a1, a2);

    case SYSCALL_NET_PACKET_COUNT:
      return bdos_net_ringbuf_count();

    case SYSCALL_NET_GET_MAC:
    {
      int *mac_out = (int *)a1;
      int i;
      i = 0;
      while (i < 6)
      {
        mac_out[i] = fnp_our_mac[i];
        i = i + 1;
      }
      return 0;
    }

    /* ---- UART debug output ---- */
    case SYSCALL_UART_PRINT_CHAR:
      uart_putchar((char)a1);
      return 0;

    case SYSCALL_UART_PRINT_STR:
      uart_puts((char *)a1);
      return 0;

    case SYSCALL_FS_MKDIR:
      return brfs_create_dir((char *)a1);

    default:
      return -1;
  }
}
