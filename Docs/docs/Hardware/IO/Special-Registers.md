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
