#include "bdos.h"

/* Assembly helpers (from slot_asm.asm) */
extern void bdos_ccache(void);
extern void bdos_hw_stack_ptr_reset(void);
extern void bdos_exec_trampoline(void);
extern void bdos_resume_trampoline(int slot);
extern void bdos_pop_to_temp(void);

/* ---- Slot state arrays ---- */

int bdos_slot_status[MEM_SLOT_COUNT];
char bdos_slot_name[MEM_SLOT_COUNT][32];

unsigned int bdos_slot_saved_pc[MEM_SLOT_COUNT];
unsigned int bdos_slot_saved_regs[MEM_SLOT_COUNT * 15];
unsigned int bdos_slot_saved_hw_sp[MEM_SLOT_COUNT];
unsigned int bdos_slot_saved_hw_stack[MEM_SLOT_COUNT * 256];

int bdos_active_slot = BDOS_SLOT_NONE;

int bdos_switch_target = BDOS_SLOT_NONE;
int bdos_kill_requested = 0;

unsigned int bdos_suspend_temp_regs[15];

unsigned int bdos_loop_saved_sp = 0;
unsigned int bdos_loop_saved_bp = 0;

/* Program execution globals (used by asm trampoline) */
unsigned int bdos_run_entry = 0;
unsigned int bdos_run_stack = 0;
unsigned int bdos_run_saved_sp = 0;
unsigned int bdos_run_saved_bp = 0;
int bdos_run_retval = 0;

/* ---- Slot management functions ---- */

void bdos_slot_init(void)
{
  int i;
  for (i = 0; i < MEM_SLOT_COUNT; i++)
  {
    bdos_slot_status[i] = BDOS_SLOT_STATUS_EMPTY;
    bdos_slot_name[i][0] = 0;
    bdos_slot_saved_pc[i] = 0;
    bdos_slot_saved_hw_sp[i] = 0;
  }
  bdos_active_slot = BDOS_SLOT_NONE;
}

int bdos_slot_alloc(void)
{
  int i;
  for (i = 0; i < MEM_SLOT_COUNT; i++)
  {
    if (bdos_slot_status[i] == BDOS_SLOT_STATUS_EMPTY)
    {
      bdos_slot_status[i] = BDOS_SLOT_STATUS_RUNNING;
      bdos_slot_name[i][0] = 0;
      return i;
    }
  }
  return BDOS_SLOT_NONE;
}

void bdos_slot_free(int slot)
{
  if (slot >= 0 && slot < MEM_SLOT_COUNT)
  {
    bdos_slot_status[slot] = BDOS_SLOT_STATUS_EMPTY;
    bdos_slot_name[slot][0] = 0;
    if (bdos_active_slot == slot)
    {
      bdos_active_slot = BDOS_SLOT_NONE;
    }
    bdos_heap_free_all();
  }
}

unsigned int bdos_slot_entry_addr(int slot)
{
  return MEM_PROGRAM_START + ((unsigned int)slot * MEM_SLOT_SIZE);
}

unsigned int bdos_slot_stack_addr(int slot)
{
  return bdos_slot_entry_addr(slot) + MEM_SLOT_SIZE - 4;
}

/* ---- Save and switch (C portion) ---- */

/*
 * Called from slot_asm.asm after user registers are saved into
 * bdos_suspend_temp_regs and BDOS stack is restored.
 */
void bdos_save_and_switch_c(void)
{
  int slot;
  int total_sp;
  int user_sp;
  int i;
  int base;

  slot = bdos_active_slot;

  if (bdos_kill_requested)
  {
    bdos_hw_stack_ptr_reset();
    bdos_slot_free(slot);
    bdos_active_slot = BDOS_SLOT_NONE;
    bdos_kill_requested = 0;
    bdos_switch_target = BDOS_SLOT_NONE;

    bdos_ccache();
    term_puts("\nProgram killed\n");
    bdos_shell_reset_and_prompt();
    bdos_loop();
    return;
  }

  /* Suspend: save user HW stack entries */
  total_sp = __builtin_load(FPGC_HW_STACK_PTR);
  user_sp = total_sp - 13; /* subtract trampoline entries */
  if (user_sp < 0)
  {
    user_sp = 0;
  }
  bdos_slot_saved_hw_sp[slot] = user_sp;

  /* Copy user registers from temp to slot state */
  base = slot * 15;
  for (i = 0; i < 15; i++)
  {
    bdos_slot_saved_regs[base + i] = bdos_suspend_temp_regs[i];
  }

  /* Pop user HW stack entries (saving them, popping from top) */
  base = slot * 256;
  for (i = 0; i < user_sp; i++)
  {
    bdos_pop_to_temp();
    bdos_slot_saved_hw_stack[base + i] = bdos_suspend_temp_regs[0];
  }

  /* Discard trampoline entries */
  bdos_hw_stack_ptr_reset();

  bdos_slot_status[slot] = BDOS_SLOT_STATUS_SUSPENDED;
  bdos_active_slot = BDOS_SLOT_NONE;

  bdos_ccache();

  if (bdos_switch_target >= 0 && bdos_switch_target < MEM_SLOT_COUNT &&
      bdos_slot_status[bdos_switch_target] == BDOS_SLOT_STATUS_SUSPENDED)
  {
    int target;
    target = bdos_switch_target;
    bdos_switch_target = BDOS_SLOT_NONE;
    bdos_resume_program(target);
  }

  bdos_switch_target = BDOS_SLOT_NONE;

  term_puts("\n[");
  term_putint(slot);
  term_puts("] suspended: ");
  term_puts(bdos_slot_name[slot]);
  term_putchar('\n');

  bdos_shell_reset_and_prompt();
  bdos_loop();
}

/* ---- Program execution ---- */

int bdos_exec_program(char *resolved_path)
{
  int slot;
  int fd;
  int file_size;
  int words_remaining;
  int chunk_len;
  int words_read;
  unsigned int *dest;
  char *basename;

  slot = bdos_slot_alloc();
  if (slot == BDOS_SLOT_NONE)
  {
    term_puts("error: no free program slot\n");
    return -1;
  }

  basename = strrchr(resolved_path, '/');
  if (basename)
  {
    basename = basename + 1;
  }
  else
  {
    basename = resolved_path;
  }
  strncpy(bdos_slot_name[slot], basename, 31);
  bdos_slot_name[slot][31] = 0;

  fd = brfs_open(resolved_path);
  if (fd < 0)
  {
    bdos_shell_print_fs_error("open", fd);
    bdos_slot_free(slot);
    return -1;
  }

  file_size = brfs_file_size(fd);
  if (file_size <= 0)
  {
    term_puts("error: empty or invalid binary\n");
    brfs_close(fd);
    bdos_slot_free(slot);
    return -1;
  }

  if ((unsigned int)file_size > MEM_SLOT_SIZE / 4)
  {
    term_puts("error: binary too large for slot (max ");
    term_putint((int)(MEM_SLOT_SIZE / 4));
    term_puts(" words)\n");
    brfs_close(fd);
    bdos_slot_free(slot);
    return -1;
  }

  dest = (unsigned int *)bdos_slot_entry_addr(slot);
  words_remaining = file_size;

  while (words_remaining > 0)
  {
    chunk_len = words_remaining;
    if (chunk_len > 256)
    {
      chunk_len = 256;
    }

    words_read = brfs_read(fd, dest, (unsigned int)chunk_len);
    if (words_read < 0)
    {
      bdos_shell_print_fs_error("read", words_read);
      brfs_close(fd);
      bdos_slot_free(slot);
      return -1;
    }

    if (words_read == 0)
    {
      break;
    }

    dest = dest + words_read;
    words_remaining = words_remaining - words_read;
  }

  brfs_close(fd);

  bdos_ccache();

  /* Set up globals for the assembly trampoline */
  bdos_run_entry = bdos_slot_entry_addr(slot);
  bdos_run_stack = bdos_slot_stack_addr(slot);
  bdos_active_slot = slot;

  /* Execute via asm trampoline — returns when user program exits */
  bdos_exec_trampoline();

  bdos_ccache();

  bdos_active_slot = BDOS_SLOT_NONE;
  fnp_net_user_owned = 0;
  bdos_net_ringbuf_reset();
  bdos_slot_free(slot);

  term_puts("Program exited with code ");
  term_putint(bdos_run_retval);
  term_putchar('\n');

  return bdos_run_retval;
}

/* ---- Program resume ---- */

void bdos_resume_program(int slot)
{
  int base_regs;
  int user_sp;
  int i;

  bdos_active_slot = slot;
  bdos_slot_status[slot] = BDOS_SLOT_STATUS_RUNNING;
  bdos_switch_target = BDOS_SLOT_NONE;

  bdos_run_entry = bdos_slot_saved_pc[slot];
  user_sp = bdos_slot_saved_hw_sp[slot];

  /* Copy saved registers into temp array for assembly access */
  base_regs = slot * 15;
  for (i = 0; i < 15; i++)
  {
    bdos_suspend_temp_regs[i] = bdos_slot_saved_regs[base_regs + i];
  }

  /* The asm trampoline handles everything from here:
   * pushes BDOS regs, pushes user HW stack, pushes user regs,
   * sets PC_BACKUP, jumps to Return_Interrupt. */
  bdos_resume_trampoline(slot);

  /* Reached when user program exits normally via trampoline return */
  bdos_ccache();

  bdos_active_slot = BDOS_SLOT_NONE;
  fnp_net_user_owned = 0;
  bdos_net_ringbuf_reset();
  bdos_slot_free(slot);

  term_puts("Program exited with code ");
  term_putint(bdos_run_retval);
  term_putchar('\n');
}
