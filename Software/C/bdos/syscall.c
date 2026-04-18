#include "bdos.h"
#include "bdos_vfs.h"

/*
 * BDOS syscall dispatcher.
 *
 * shell-terminal-v2 Phase E: a number of legacy syscalls were removed.
 * Their slot numbers are reserved in bdos_syscall.h. The dispatcher
 * falls through to `default: -1` for those slots, so user programs
 * that still call the old wrappers get a clear error rather than
 * undefined behaviour. See the header for the migration table.
 */

/* Assembly helper for EXIT syscall (from slot_asm.asm) */
extern void bdos_syscall_exit(int exit_code);

int bdos_syscall_dispatch(int num, int a1, int a2, int a3)
{
  switch (num)
  {
    /* ---- Filesystem (raw BRFS — kept for tools that want word-level I/O) ---- */
    case SYSCALL_FS_OPEN:      return brfs_open((char *)a1);
    case SYSCALL_FS_CLOSE:     return brfs_close(a1);
    case SYSCALL_FS_READ:      return brfs_read(a1, (void *)a2, (unsigned int)a3);
    case SYSCALL_FS_WRITE:     return brfs_write(a1, (const void *)a2, (unsigned int)a3);
    case SYSCALL_FS_SEEK:      return brfs_seek(a1, (unsigned int)a2);
    case SYSCALL_FS_STAT:      return brfs_stat((char *)a1, (struct brfs_dir_entry *)a2);
    case SYSCALL_FS_DELETE:    return brfs_delete((char *)a1);
    case SYSCALL_FS_CREATE:    return brfs_create_file((char *)a1);
    case SYSCALL_FS_FILESIZE:  return brfs_file_size(a1);
    case SYSCALL_FS_READDIR:   return brfs_read_dir((char *)a1, (struct brfs_dir_entry *)a2, (unsigned int)a3);
    case SYSCALL_FS_MKDIR:     return brfs_create_dir((char *)a1);

    case SYSCALL_FS_FORMAT:
      /* args: a1 = blocks, a2 = words_per_block, a3 = label_ptr.
         Performs format + sync; the userland `format` tool wraps this. */
      return bdos_fs_format_and_sync((unsigned int)a1,
                                     (unsigned int)a2,
                                     (char *)a3,
                                     1 /* full format */);

    /* ---- Shell integration ---- */
    case SYSCALL_SHELL_ARGC:   return bdos_shell_prog_argc;
    case SYSCALL_SHELL_ARGV:   return (int)bdos_shell_prog_argv;
    case SYSCALL_SHELL_GETCWD: return (int)bdos_shell_cwd;

    /* ---- Heap ---- */
    case SYSCALL_HEAP_ALLOC:   return (int)bdos_heap_alloc((unsigned int)a1);

    /* ---- Timing ---- */
    case SYSCALL_DELAY:        delay((unsigned int)a1); return 0;

    /* ---- Process control ---- */
    case SYSCALL_EXIT:         bdos_syscall_exit(a1); return 0; /* unreachable */

    /* ---- Key state (held-key bitmap, no fd equivalent) ---- */
    case SYSCALL_GET_KEY_STATE: return (int)bdos_key_state_bitmap;

    /* ---- Networking (raw Ethernet) ---- */
    case SYSCALL_NET_SEND:
      fnp_net_user_owned = 1;
      return enc28j60_packet_send((char *)a1, a2);

    case SYSCALL_NET_RECV:
      fnp_net_user_owned = 1;
      return bdos_net_ringbuf_pop((char *)a1, a2);

    case SYSCALL_NET_PACKET_COUNT: return bdos_net_ringbuf_count();

    case SYSCALL_NET_GET_MAC:
    {
      int *mac_out = (int *)a1;
      int i;
      for (i = 0; i < 6; i++) mac_out[i] = fnp_our_mac[i];
      return 0;
    }

    /* ---- VFS / fd-oriented byte I/O (Phase B) ---- */
    case SYSCALL_OPEN:   return bdos_vfs_open((const char *)a1, a2);
    case SYSCALL_READ:   return bdos_vfs_read(a1, (void *)a2, a3);
    case SYSCALL_WRITE:  return bdos_vfs_write(a1, (const void *)a2, a3);
    case SYSCALL_CLOSE:  return bdos_vfs_close(a1);
    case SYSCALL_LSEEK:  return bdos_vfs_lseek(a1, a2, a3);
    case SYSCALL_DUP2:   return bdos_vfs_dup2(a1, a2);

    default:
      return -1;
  }
}
