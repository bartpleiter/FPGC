; Prints a prompt, then echoes five bytes, polling the UART's status. The receiver only
; listens after the line has been high for a whole byte after the reset, so the input
; starts after cycle 1000.
; devices
; uart input "abcde" at cycle 1200
; uart output "> abcde"
    load 0x3E r1
    write 0xFFFF8000 r0 r1      ; TRANSMIT
    load 0x20 r1
    write 0xFFFF8000 r0 r1
    load 5 r2
Wait:
    read 0xFFFF8008 r0 r3       ; STATUS
    and r3 1 r3                 ; a byte is waiting
    beq r3 r0 Wait
    read 0xFFFF8004 r0 r1       ; RECEIVE
    write 0xFFFF8000 r0 r1
    sub r2 1 r2
    bne r2 r0 Wait
    halt
