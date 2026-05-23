/*
 * i2c.h — Generic I2C master driver for FPGC
 *
 * Provides single-byte register read/write over I2C.
 * The hardware supports any 7-bit device address.
 */
#ifndef FPGC_I2C_H
#define FPGC_I2C_H

#include "fpgc.h"

/*
 * Write a single byte to an I2C device register.
 *
 *   dev_addr: 7-bit I2C device address (e.g. 0x21 for OV7670)
 *   reg:      8-bit register address
 *   data:     8-bit value to write
 *
 * Blocks until the transaction completes. Returns 0 on success,
 * -1 if the slave did not ACK.
 */
int i2c_write(int dev_addr, int reg, int data);

/*
 * Read a single byte from an I2C device register.
 *
 *   dev_addr: 7-bit I2C device address
 *   reg:      8-bit register address
 *
 * Blocks until the transaction completes. Returns the read byte
 * (0-255) on success, or -1 if the slave did not ACK.
 */
int i2c_read(int dev_addr, int reg);

/* Returns non-zero while the I2C master is busy. */
int i2c_busy(void);

#endif /* FPGC_I2C_H */
