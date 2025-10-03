# Bootloaders

The FPGC always boots from ROM, which can only be modified by reprogramming the FPGA. This is something we basically never want to do. Instead, you want to be able to run your own code from one of the following locations:

- **SPI Flash** (e.g. for long term code like an Operating System)
- **UART** (e.g. for development and testing of bare metal programs)
- **SD Card** (e.g. for user programs to run from an Operating System)
- **Network** (e.g. for development and testing of user programs run from an Operating System)

To run programs from an SD Card or Network, it is best to have an Operating System already running on the FPGC. As an OS will likely not fit in the limited ROM space, and since we most likely want to be able to update the OS, we will need to load the OS from SPI Flash (long term) or UART (development or testing) first. Therefore, we will need a bootloader in ROM that supports booting from SPI Flash and UART.

## ROM Bootloader

!!! warning
    The ROM bootloader is still under development, so I'll write here how I want it to be, even though I still have to implement it.

In short, the ROM bootloader does the following:

1. Clear all registers and cache, in case a reset was performed which does not clear the FPGA BRAMs
2. Display a boot logo using the tile renderer
3. Check using the Memory Unit what the position is of the boot dipswitch
4. Depending on the dipswitch position:
    - If set to SPI Flash, read the code length from a fixed position in SPI Flash, then copy the code from SPI Flash to RAM, and jump to it
    - If set to UART, copy the UART bootloader code from ROM to RAM, and jump to it

### UART Bootloader

The UART bootloader needs to run from RAM, as it needs the UART RX interrupt handler to work, and interrupts are disabled in ROM. The UART bootloader works as follows:

1. Halt at address 0, as this keeps the program counter at 0. All bootloader code runs from interrupts
2. The host needs to send 4 bytes from most significant to least significant byte, representing the length of the code in words to be sent. This length is stored in a register
3. After the length has been received, bootloader sends back the same 4 bytes as an acknowledgment/verification
4. The host then sends the code in the same order (most significant to least significant word), while the bootloader stores the code in RAM starting from address 0. Note that this works because UART bootloader fits in l1i cache, so the halt at address 0 is not overwritten until a ccache instruction is used
5. After the code has been received, the bootloader sends a single 'd' character as an acknowledgment
6. The bootloader resets all registers, clears the cache, and returns from the interrupt, which jumps to address 0 and starts executing the received code

TODO: write or reference the guide on how to send a program over UART.

## OS Bootloaders

TODO: once BDOS has implemented booting from SD Card and Network, document how this is done, or move this section to the BDOS documentation.
