/*
 * i2c.c — Generic I2C master driver implementation
 */
#include "i2c.h"
#include "uart.h"
#include "dma.h"

int
i2c_write(int dev_addr, int reg, int data)
{
    int cmd;
    int status;
    int timeout;

    /* Wait for any previous transaction to finish */
    timeout = 0;
    do {
        status = __builtin_load(FPGC_I2C_CMD);
        timeout = timeout + 1;
        if (timeout > 1000000) {
            uart_puts("I2C:pre-busy stuck\n");
            return -2;
        }
    } while (status & 1);  /* bit 0 = busy */

    /* Build command word: {dev_addr[6:0], rw=0, reg[7:0], data[7:0]} */
    cmd = ((dev_addr & 0x7F) << 17) | (0 << 16) | ((reg & 0xFF) << 8) | (data & 0xFF);
    __builtin_store(FPGC_I2C_CMD, cmd);

    /* Flush data cache so subsequent reads bypass the stale cached write value */
    cache_flush_data();

    /* Wait for completion */
    timeout = 0;
    do {
        status = __builtin_load(FPGC_I2C_CMD);
        timeout = timeout + 1;
        if (timeout > 1000000) {
            uart_puts("I2C:stuck s=");
            uart_puthex(status, 0);
            uart_puts(" d=");
            uart_puthex(__builtin_load(0x1C000090), 0);
            uart_putchar('\n');
            return -3;
        }
    } while (status & 1);

    /* Check ACK error (bit 1) */
    if (status & 2)
        return -1;
    return 0;
}

int
i2c_read(int dev_addr, int reg)
{
    int cmd;
    int status;
    int rd;
    int timeout;

    /* Wait for any previous transaction to finish */
    timeout = 0;
    do {
        status = __builtin_load(FPGC_I2C_CMD);
        timeout = timeout + 1;
        if (timeout > 1000000) return -2;
    } while (status & 1);

    /* Build command word: {dev_addr[6:0], rw=1, reg[7:0], data=0} */
    cmd = ((dev_addr & 0x7F) << 17) | (1 << 16) | ((reg & 0xFF) << 8);
    __builtin_store(FPGC_I2C_CMD, cmd);

    /* Flush data cache so subsequent reads bypass stale cached write value */
    cache_flush_data();

    /* Wait for completion */
    timeout = 0;
    do {
        status = __builtin_load(FPGC_I2C_CMD);
        timeout = timeout + 1;
        if (timeout > 1000000) return -3;
    } while (status & 1);

    /* Check ACK error */
    if (status & 2)
        return -1;

    /* Read received byte */
    rd = __builtin_load(FPGC_I2C_DATA);
    return rd & 0xFF;
}

int
i2c_busy(void)
{
    return __builtin_load(FPGC_I2C_CMD) & 1;
}
