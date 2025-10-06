# Special Registers

There are a number of special registers available for the CPU to read or write.

## Boot mode

The boot mode register is a read only register that contains a single bit, corresponding to a dip switch position on the I/O board.
This register is used by the ROM bootloader to check if we need to boot from UART or from SPI Flash.

## Micros

The micros register is a read only register that contains the amount of microseconds since boot or last reset. As it is a 32 bit unsigned register, it overflows after ~71.5 minutes. Therfore, it is important to use proper software techniques that are robust to overflows, like the following example:

```c
// Assuming now and start are signed
if ((now - start) >= 1000000) {
    // one second has passed
}
```

## GPIO

The FPGC contains 8 GPIO pins that can be accessed through two GPIO registers. The GPIO Mode register contains one bit for each pin to set the pin to Input (0) or Output (1). The configured mode can also be read back. Reading the GPIO State register returns the digital state of each pin as bits, while writing this register sets the ouput value for those pins that are configured as output. On boot or reset all pins are configured as input.

TODO: Indicate for each pin which bit it corresponds to

## FPGA temperature

The FPGA temp register is a read only register that reads the die temperature from the Artix 7 FPGA, which is possible because there is an internal temperature sensor and ADC within the Artix 7.
