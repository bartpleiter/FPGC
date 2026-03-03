//
// BDOS program slot management and execution.
//
// Handles slot allocation, program loading/execution, suspend/resume,
// and the save-and-switch routine for multitasking.
//

#include "BDOS/bdos.h"

// ---- Slot state arrays ----

int bdos_slot_status[MEM_SLOT_COUNT];
char bdos_slot_name[MEM_SLOT_COUNT][BDOS_SLOT_NAME_MAX];

// Multitasking saved state
unsigned int bdos_slot_saved_pc[MEM_SLOT_COUNT];
unsigned int bdos_slot_saved_regs[MEM_SLOT_COUNT * 15];
unsigned int bdos_slot_saved_hw_sp[MEM_SLOT_COUNT];
unsigned int bdos_slot_saved_hw_stack[MEM_SLOT_COUNT * 256];

// Currently active slot
int bdos_active_slot = BDOS_SLOT_NONE;

// Multitasking switch/kill request flags
int bdos_switch_target = BDOS_SLOT_NONE;
int bdos_kill_requested = 0;

// Temp register save area
unsigned int bdos_suspend_temp_regs[15];

// BDOS loop stack state
unsigned int bdos_loop_saved_sp = 0;
unsigned int bdos_loop_saved_bp = 0;

// ---- Slot management functions ----

void bdos_slot_init()
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

int bdos_slot_alloc()
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
    // Free all heap allocations made by the program
    bdos_heap_free_all();
  }
}

unsigned int bdos_slot_entry_addr(int slot)
{
  return MEM_PROGRAM_START + ((unsigned int)slot * MEM_SLOT_SIZE);
}

unsigned int bdos_slot_stack_addr(int slot)
{
  return bdos_slot_entry_addr(slot) + MEM_SLOT_SIZE - 1;
}

// ---- Program execution globals ----
// Used by the inline assembly trampoline in bdos_exec_program.
unsigned int bdos_run_entry = 0;
unsigned int bdos_run_stack = 0;
unsigned int bdos_run_saved_sp = 0;
unsigned int bdos_run_saved_bp = 0;
int bdos_run_retval = 0;

// ---- Save-and-switch routine ----
// Entered via reti redirect when a program is being suspended or killed.
// At this point, Return_Interrupt has popped r1-r15 from HW stack,
// so CPU registers hold the user program's state.
// HW stack contains: [trampoline:13 entries] [user call chain]
//
// This function:
// 1. Saves user registers to temp array
// 2. Switches to BDOS stack
// 3. Saves user HW stack entries (excluding trampoline)
// 4. Handles kill or suspend
// 5. Returns to BDOS main loop
void bdos_save_and_switch()
{
  int slot;
  int total_sp;
  int user_sp;
  int i;
  int base;

  // Step 1: Save all user registers and switch to BDOS stack.
  // IMPORTANT: The C compiler generates a function prologue before this asm block
  // that modifies r13 (sub r13 N r13) and r14 (via save/set). We must undo these
  // to get the user's REAL register values before saving.
  asm(
    "read 0 r14 r14"
    "add r13 7 r13"
    "push r1"
    "addr2reg Label_bdos_suspend_temp_regs r1"
    "write 1 r1 r2"
    "write 2 r1 r3"
    "write 3 r1 r4"
    "write 4 r1 r5"
    "write 5 r1 r6"
    "write 6 r1 r7"
    "write 7 r1 r8"
    "write 8 r1 r9"
    "write 9 r1 r10"
    "write 10 r1 r11"
    "write 11 r1 r12"
    "write 12 r1 r13"
    "write 13 r1 r14"
    "write 14 r1 r15"
    "pop r2"
    "write 0 r1 r2"
    "addr2reg Label_bdos_loop_saved_sp r1"
    "read 0 r1 r13"
    "addr2reg Label_bdos_loop_saved_bp r1"
    "read 0 r1 r14"
  );

  // Now running with BDOS stack. User registers are in bdos_suspend_temp_regs.
  slot = bdos_active_slot;

  if (bdos_kill_requested)
  {
    // Kill: discard everything, just clear the HW stack
    asm(
      "load32 0x7C00001 r1"
      "write 0 r1 r0"
    );
    bdos_slot_free(slot);
    bdos_active_slot = BDOS_SLOT_NONE;
    bdos_kill_requested = 0;
    bdos_switch_target = BDOS_SLOT_NONE;

    asm("ccache");
    term_puts("\nProgram killed\n");
    bdos_shell_reset_and_prompt();
    bdos_loop();
    return;
  }

  // Suspend: save user HW stack entries
  // Read current HW stack pointer
  total_sp = *(volatile unsigned int*)MEM_IO_HW_STACK_PTR;
  user_sp = total_sp - 13; // subtract trampoline entries
  if (user_sp < 0)
  {
    user_sp = 0;
  }
  bdos_slot_saved_hw_sp[slot] = user_sp;

  // Copy user registers from temp to slot state
  base = slot * 15;
  for (i = 0; i < 15; i++)
  {
    bdos_slot_saved_regs[base + i] = bdos_suspend_temp_regs[i];
  }

  // Pop user HW stack entries (saving them, popping from top)
  // After this, only the 13 trampoline entries remain
  base = slot * 256;
  for (i = 0; i < user_sp; i++)
  {
    asm(
      "pop r1"
      "addr2reg Label_bdos_suspend_temp_regs r2"
      "write 0 r2 r1"
    );
    bdos_slot_saved_hw_stack[base + i] = bdos_suspend_temp_regs[0];
  }

  // Discard trampoline entries by resetting HW stack pointer
  asm(
    "load32 0x7C00001 r1"
    "write 0 r1 r0"
  );

  bdos_slot_status[slot] = BDOS_SLOT_STATUS_SUSPENDED;
  bdos_active_slot = BDOS_SLOT_NONE;

  asm("ccache");

  if (bdos_switch_target >= 0 && bdos_switch_target < MEM_SLOT_COUNT &&
      bdos_slot_status[bdos_switch_target] == BDOS_SLOT_STATUS_SUSPENDED)
  {
    // Direct slot-to-slot switch: resume target slot immediately
    int target;
    target = bdos_switch_target;
    bdos_switch_target = BDOS_SLOT_NONE;
    bdos_resume_program(target);
    // Program exited normally, fall through to shell
  }

  bdos_switch_target = BDOS_SLOT_NONE;

  term_puts("\n[");
  term_putint(slot);
  term_puts("] suspended: ");
  term_puts(bdos_slot_name[slot]);
  term_putchar('\n');

  // Redraw shell prompt and return to BDOS main loop
  bdos_shell_reset_and_prompt();
  bdos_loop();
}

// ---- Program execution ----

// Load and execute a program binary from a resolved BRFS path.
// Allocates a program slot, loads the binary, runs it to completion,
// then frees the slot. Returns the program's exit code, or -1 on error.
int bdos_exec_program(char* resolved_path)
{
  int slot;
  int fd;
  int file_size;
  int words_remaining;
  int chunk_len;
  int words_read;
  unsigned int *dest;
  char* basename;

  slot = bdos_slot_alloc();
  if (slot == BDOS_SLOT_NONE)
  {
    term_puts("error: no free program slot\n");
    return -1;
  }

  // Extract basename for display in jobs list
  basename = strrchr(resolved_path, '/');
  if (basename)
  {
    basename = basename + 1;
  }
  else
  {
    basename = resolved_path;
  }
  strncpy(bdos_slot_name[slot], basename, BDOS_SLOT_NAME_MAX - 1);
  bdos_slot_name[slot][BDOS_SLOT_NAME_MAX - 1] = 0;
  if (slot == BDOS_SLOT_NONE)
  {
    term_puts("error: no free program slot\n");
    return -1;
  }

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

  if ((unsigned int)file_size > MEM_SLOT_SIZE)
  {
    term_puts("error: binary too large for slot (max ");
    term_putint((int)MEM_SLOT_SIZE);
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

  // Flush the instruction cache since we just wrote new code to RAM
  asm("ccache");

  // Set up globals for the inline assembly trampoline
  bdos_run_entry = bdos_slot_entry_addr(slot);
  bdos_run_stack = bdos_slot_stack_addr(slot);
  bdos_active_slot = slot;

  // Execute the user program via inline assembly.
  // We save all BDOS registers to the stack (except r13/r14 which go
  // to globals, since the user program gets its own stack).
  // The program's entry point at slot offset 0 is the ASMPY header
  // "jump Main".  When the user's main() returns, r15 brings execution
  // back to Label_bdos_run_return.  Return value is in r1.
  asm(
      "push r1"
      "push r2"
      "push r3"
      "push r4"
      "push r5"
      "push r6"
      "push r7"
      "push r8"
      "push r9"
      "push r10"
      "push r11"
      "push r12"
      "push r15"
      "addr2reg Label_bdos_run_saved_sp r11"
      "write 0 r11 r13                       ; save BDOS stack pointer"
      "addr2reg Label_bdos_run_saved_bp r11"
      "write 0 r11 r14                       ; save BDOS base pointer"
      "addr2reg Label_bdos_run_entry r11"
      "read 0 r11 r11                        ; r11 = user program entry"
      "addr2reg Label_bdos_run_stack r12"
      "read 0 r12 r13                        ; r13 = user program stack"
      "addr2reg Label_bdos_run_return r15     ; r15 = return-to-BDOS address"
      "jumpr 0 r11                            ; jump to user program"
      "Label_bdos_run_return:"
      "addr2reg Label_bdos_run_retval r11"
      "write 0 r11 r1                         ; save user return value"
      "addr2reg Label_bdos_run_saved_sp r11"
      "read 0 r11 r13                         ; restore BDOS stack pointer"
      "addr2reg Label_bdos_run_saved_bp r11"
      "read 0 r11 r14                         ; restore BDOS base pointer"
      "pop r15"
      "pop r12"
      "pop r11"
      "pop r10"
      "pop r9"
      "pop r8"
      "pop r7"
      "pop r6"
      "pop r5"
      "pop r4"
      "pop r3"
      "pop r2"
      "pop r1"
  );

  // Flush cache again after user program execution
  asm("ccache");

  bdos_active_slot = BDOS_SLOT_NONE;
  fnp_net_user_owned = 0;
  bdos_slot_free(slot);

  term_puts("Program exited with code ");
  term_putint(bdos_run_retval);
  term_putchar('\n');

  return bdos_run_retval;
}

// ---- Program resume ----

// Resume a suspended program in the given slot.
// Pushes fresh trampoline entries, saved user HW stack, and saved interrupt
// registers, then jumps to Return_Interrupt which restores user registers
// and reti to the saved PC. When the user program eventually exits normally,
// the trampoline return (Label_bdos_run_return) brings execution back to
// the cleanup code after the asm block below.
void bdos_resume_program(int slot)
{
  int base_regs;
  int base_hw;
  int user_sp;
  int i;

  bdos_active_slot = slot;
  bdos_slot_status[slot] = BDOS_SLOT_STATUS_RUNNING;
  bdos_switch_target = BDOS_SLOT_NONE;

  // Load saved state into globals for the push loops
  bdos_run_entry = bdos_slot_saved_pc[slot];
  user_sp = bdos_slot_saved_hw_sp[slot];

  // Copy saved registers into temp array for assembly access
  base_regs = slot * 15;
  for (i = 0; i < 15; i++)
  {
    bdos_suspend_temp_regs[i] = bdos_slot_saved_regs[base_regs + i];
  }

  // Inline assembly: save BDOS state, push saved context, resume user
  asm(
    "; Save BDOS registers to HW stack"
    "push r1"
    "push r2"
    "push r3"
    "push r4"
    "push r5"
    "push r6"
    "push r7"
    "push r8"
    "push r9"
    "push r10"
    "push r11"
    "push r12"
    "push r15"
    "addr2reg Label_bdos_run_saved_sp r11"
    "write 0 r11 r13"
    "addr2reg Label_bdos_run_saved_bp r11"
    "write 0 r11 r14"

    "; Push saved user HW stack entries (reverse order to restore original stack)"
    "; user_sp entries saved in pop order: top-first at index 0"
    "; Push from index user_sp-1 down to 0 to restore bottom-first"
    "addr2reg Label_bdos_slot_saved_hw_sp r1"
    "addr2reg Label_bdos_active_slot r2"
    "read 0 r2 r2"
    "add r1 r2 r1"
    "read 0 r1 r3"

    "addr2reg Label_bdos_slot_saved_hw_stack r1"
    "shiftl r2 8 r4"
    "add r1 r4 r1"

    "; r1 = base addr, r3 = user_sp (count), r4 = index (starts at user_sp-1)"
    "sub r3 1 r4"
    "bdos_resume_push_user_loop:"
    "beq r3 r0 7"
    "add r1 r4 r5"
    "read 0 r5 r5"
    "push r5"
    "sub r4 1 r4"
    "sub r3 1 r3"
    "jump bdos_resume_push_user_loop"
    "bdos_resume_push_user_done:"

    "; Push saved interrupt registers (user r1-r15)"
    "; temp_regs[0] = r1, temp_regs[1] = r2, ..., temp_regs[14] = r15"
    "; Must push in Int: order: r1, r2, ..., r15"
    "; So Return_Interrupt can pop correctly: r15, r14, ..., r1"
    "addr2reg Label_bdos_suspend_temp_regs r1"
    "read 0 r1 r2"
    "push r2"
    "read 1 r1 r2"
    "push r2"
    "read 2 r1 r2"
    "push r2"
    "read 3 r1 r2"
    "push r2"
    "read 4 r1 r2"
    "push r2"
    "read 5 r1 r2"
    "push r2"
    "read 6 r1 r2"
    "push r2"
    "read 7 r1 r2"
    "push r2"
    "read 8 r1 r2"
    "push r2"
    "read 9 r1 r2"
    "push r2"
    "read 10 r1 r2"
    "push r2"
    "read 11 r1 r2"
    "push r2"
    "read 12 r1 r2"
    "push r2"
    "read 13 r1 r2"
    "push r2"
    "read 14 r1 r2"
    "push r2"

    "; Set IO_PC_BACKUP to saved user PC"
    "addr2reg Label_bdos_run_entry r1"
    "read 0 r1 r1"
    "load32 0x7C00000 r2"
    "write 0 r2 r1"

    "; Jump to Return_Interrupt: pops r15..r1, reti -> user program"
    "jump Return_Interrupt"
  );

  // This point is reached when the user program exits normally.
  // The trampoline return (Label_bdos_run_return) restores r13/r14
  // from bdos_run_saved_sp/bp and pops the 13 trampoline entries
  // we pushed above, then returns here via r15.

  asm("ccache");

  bdos_active_slot = BDOS_SLOT_NONE;
  fnp_net_user_owned = 0;
  bdos_slot_free(slot);

  term_puts("Program exited with code ");
  term_putint(bdos_run_retval);
  term_putchar('\n');
}
