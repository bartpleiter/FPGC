#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_TIMER
#define KERNEL_CH376
#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#include "libs/kernel/kernel.h"

void init()
{
  // Reset GPU VRAM
  gpu_clear_vram();

  // Load default pattern and palette tables
  unsigned int *pattern_table = (unsigned int *)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  unsigned int *palette_table = (unsigned int *)&DATA_PALETTE_DEFAULT;
  gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

  // Initialize terminal
  term_init();
}

int main()
{
  init();
  term_puts("CH376 lib test\n");

  // TODO: Run several functions of the CH376 library to verify functionality against actual hardware
  
  // Should include a demo that reads and prints the device descriptor of the connected USB keyboard
  // and eventually reads the key presses from it.

  return 1;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
  case INTID_TIMER2:
    // Timer used for delay()
    timer_isr_handler(TIMER_2);
    break;
  default:
    break;
  }
}
