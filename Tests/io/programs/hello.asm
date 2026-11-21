; Prints "hello" over the UART, with the device bridge and the devices on the bus. The
; text is in the ROM, one character per word, and is read over the bus, from the ROM's
; data port. Each write goes into the UART's buffer at once.
; devices
; uart output "hello\n"
    addr2reg Text r1
    load 6 r2
Next:
    read 0 r1 r3
    write 0xFFFF8000 r0 r3      ; TRANSMIT
    add r1 4 r1
    sub r2 1 r2
    bne r2 r0 Next
    halt
Text:
    .dw 0x68 0x65 0x6C 0x6C 0x6F 0x0A
