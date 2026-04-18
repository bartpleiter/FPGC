#include "bdos.h"

/* Assembly helpers (from slot_asm.asm) */
extern void bdos_halt(void);
extern void bdos_ccache(void);
extern void bdos_loop_save_sp_bp(void);
extern void bdos_interrupt_redirect_pc(void);
extern unsigned int bdos_read_pc_backup(void);

void bdos_panic(char *msg)
{
  term2_set_palette(PALETTE_WHITE_ON_RED);
  term2_puts("BDOS PANIC:\n");
  term2_puts(msg);
  term2_puts("\n\nSystem halted.\n");

  uart_puts("BDOS PANIC:\n");
  uart_puts(msg);
  uart_puts("\n\nSystem halted.\n");

  bdos_halt();
}

void bdos_loop(void)
{
  /* Save stack state for clean recovery after program suspend. */
  bdos_loop_save_sp_bp();

  while (1)
  {
    bdos_usb_keyboard_main_loop();
    bdos_fnp_poll();
    bdos_shell_tick();
  }
}

int main(void)
{
  bdos_init();
  bdos_fs_boot_init();
  bdos_shell_init();
  bdos_loop();

  return 0x42;
}

void interrupt(void)
{
  int int_id;

  int_id = get_int_id();
  switch (int_id)
  {
    case INTID_UART:
      break;
    case INTID_TIMER0:
      if (net_isr_deferred)
      {
        if (enc28j60_spi_in_use)
        {
          timer_set(TIMER_0, 1);
          timer_start(TIMER_0);
        }
        else
        {
          net_isr_deferred = 0;
          bdos_net_isr_drain();
        }
      }
      break;
    case INTID_TIMER1:
      timer_isr_handler(TIMER_1);
      break;
    case INTID_TIMER2:
      timer_isr_handler(TIMER_2);
      break;
    case INTID_FRAME_DRAWN:
      break;
    case INTID_ETH:
      if (enc28j60_spi_in_use)
      {
        net_isr_deferred = 1;
        timer_set(TIMER_0, 1);
        timer_start(TIMER_0);
      }
      else
      {
        bdos_net_isr_drain();
      }
      break;
    default:
      break;
  }

  /* Check for multitasking switch/kill requests */
  if (bdos_active_slot != BDOS_SLOT_NONE)
  {
    if (bdos_kill_requested)
    {
      bdos_slot_saved_pc[bdos_active_slot] = bdos_read_pc_backup();
      bdos_interrupt_redirect_pc();
    }
    else if (bdos_switch_target != BDOS_SLOT_NONE)
    {
      bdos_slot_saved_pc[bdos_active_slot] = bdos_read_pc_backup();
      bdos_interrupt_redirect_pc();
    }
  }
}
