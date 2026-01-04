/*
 * BDOS main module and entry point.
 */

// Include common libraries
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#include "libs/common/common.h"

// Include kernel libraries
#define KERNEL_GPU_HAL
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_TERM
#define KERNEL_UART
#define KERNEL_SPI
#define KERNEL_SPI_FLASH
#define KERNEL_BRFS
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

// Main entry point
int main()
{

  // Return value gets printed over UART
  // Should not reach here in normal operation
  return 0x42;
}


// Interrupt handler
void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_UART:
      break;
    case INTID_TIMER0:
      break;
    case INTID_TIMER1:
      break;
    case INTID_TIMER2:
      break;
    case INTID_FRAME_DRAWN:
      break;
    default:
      break;
  }
}
