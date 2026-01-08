#include "libs/kernel/io/ch376.h"
#include "libs/kernel/io/spi.h"

#define CH376_CMD_GET_IC_VER        0x01
#define CH376_CMD_ENTER_SLEEP       0x03
#define CH376_CMD_RESET_ALL         0x05
#define CH376_CMD_CHECK_EXIST       0x06
#define CH376_CMD_SET_SD0_INT       0x0B
#define CH376_CMD_SET_USB_MODE      0x15
#define CH376_CMD_GET_STATUS        0x22
#define CH376_CMD_RD_USB_DATA0      0x27
#define CH376_CMD_WR_USB_DATA       0x2C
#define CH376_CMD_WR_REQ_DATA       0x2D
#define CH376_CMD_WR_OFS_DATA       0x2E

#define CH376_MODE_INIT             0x00
#define CH376_MODE_INVALID_HOST     0x04
#define CH376_MODE_HOST             0x05
#define CH376_MODE_HOST_SOF         0x06
#define CH376_MODE_HOST_RESET       0x07

#define CH376_INT_SUCCESS           0x14
#define CH376_INT_CONNECT           0x15
#define CH376_INT_DISCONNECT        0x16
#define CH376_INT_BUF_OVER          0x17
#define CH376_INT_USB_READY         0x18

// Read interrupt pin state
// Note: inverted logic of hardware pin is corrected here
int ch376_read_int(int spi_id)
{
  // Read interrupt pin state
  if (spi_id == CH376_SPI_TOP)
  {
    // TODO
  }
  if (spi_id == CH376_SPI_BOTTOM)
  {
    // TODO
  }
  return 0;
}

void ch376_write_cmd(int spi_id, int cmd)
{
  // TODO
}

int ch376_init(int spi_id)
{
  // Send reset command
  // Delay (?ms)
  // Check if the device is ready
  return 0; // Success
}
