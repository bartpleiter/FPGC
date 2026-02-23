//
// BDOS main module and entry point.
//

#include "BDOS/bdos.h"

// Include BDOS code modules
#include "BDOS/init.c"
#include "BDOS/hid.c"
#include "BDOS/fs.c"
#include "BDOS/eth.c"
#include "BDOS/syscall.c"
#include "BDOS/slot.c"
#include "BDOS/shell_path.c"
#include "BDOS/shell_util.c"
#include "BDOS/shell_format.c"
#include "BDOS/shell_cmds.c"
#include "BDOS/shell.c"

// Print panic message to terminal and UART
void bdos_panic(char* msg)
{
  term_set_palette(PALETTE_WHITE_ON_RED);
  term_puts("BDOS PANIC:\n");
  term_puts(msg);
  term_puts("\n\nSystem halted.\n");

  uart_puts("BDOS PANIC:\n");
  uart_puts(msg);
  uart_puts("\n\nSystem halted.\n");

  asm("halt");
}

// Main BDOS control loop after initialization
void bdos_loop()
{
  // Save stack state for clean recovery after program suspend.
  // When a program is suspended, we restore these values to get a clean
  // BDOS stack without accumulated stack frame leaks.
  // Sadly this means we cannot move this inline assembly into a separate function.
  asm(
    "addr2reg Label_bdos_loop_saved_sp r1"
    "write 0 r1 r13"
    "addr2reg Label_bdos_loop_saved_bp r1"
    "write 0 r1 r14"
  );

  while (1)
  {
    bdos_usb_keyboard_main_loop();
    bdos_fnp_poll();
    bdos_shell_tick();
  }
}

// Entry point from bootloader
int main()
{
  bdos_init();
  bdos_fs_boot_init();
  bdos_shell_init();
  bdos_loop();

  // Should not reach here in normal operation
  // Return value gets printed over UART
  return 0x42;
}

// Interrupt handler
void interrupt()
{
  int int_id;

  int_id = get_int_id();
  switch (int_id)
  {
    case INTID_UART:
      break;
    case INTID_TIMER0:
      break;
    case INTID_TIMER1:
      // Used for USB keyboard polling
      timer_isr_handler(TIMER_1);
      break;
    case INTID_TIMER2:
      // Used for delay()
      timer_isr_handler(TIMER_2);
      break;
    case INTID_FRAME_DRAWN:
      break;
    default:
      break;
  }

  // Check for multitasking switch/kill requests
  if (bdos_active_slot != BDOS_SLOT_NONE)
  {
    if (bdos_kill_requested)
    {
      // Kill: redirect to save_and_switch
      bdos_slot_saved_pc[bdos_active_slot] = *(volatile unsigned int*)MEM_IO_PC_BACKUP;
      asm(
        "addr2reg Label_bdos_save_and_switch r1"
        "load32 0x7C00000 r2"
        "write 0 r2 r1"
      );
    }
    else if (bdos_switch_target != BDOS_SLOT_NONE)
    {
      // Suspend: save user's PC, redirect to save_and_switch
      bdos_slot_saved_pc[bdos_active_slot] = *(volatile unsigned int*)MEM_IO_PC_BACKUP;
      asm(
        "addr2reg Label_bdos_save_and_switch r1"
        "load32 0x7C00000 r2"
        "write 0 r2 r1"
      );
    }
  }
}
