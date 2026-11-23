; Hello world, a program that the boot ROM loads over UART: prints a line and
; returns to the boot ROM.

    addr2reg Text r1
Next:
    read 0 r1 r2
    beq r2 r0 Done
    write 0xFFFF8000 r0 r2      ; UART TRANSMIT
    add r1 4 r1
    jump Next
Done:
    jump 0x04000000             ; back to the boot ROM

Text:          ; "hello, world\r\n"
    .dw 0x68 0x65 0x6C 0x6C 0x6F 0x2C 0x20 0x77 0x6F 0x72 0x6C 0x64 0x0D 0x0A 0
