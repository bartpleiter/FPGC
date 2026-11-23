; Hello world, a program that the boot ROM loads over UART: prints a line and
; returns to the boot ROM.

.equ UART_TRANSMIT 0xFFFF8000
.equ BOOT_ROM 0x04000000

    addr2reg Text r1
Next:
    readbu 0 r1 r2
    beq r2 r0 Done
    write UART_TRANSMIT r0 r2
    add r1 1 r1
    jump Next
Done:
    jump BOOT_ROM

Text:
    .ds "hello, world\r\n"
