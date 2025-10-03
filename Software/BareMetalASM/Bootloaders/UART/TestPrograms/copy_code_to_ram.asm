Main:

    load32 17 r1 ; Program length
    addr2reg Program_RAM r2
    load 0 r3  ; Loop counter and dest address

    CopyLoop:
        read 0 r2 r4 ; Read word from Program_RAM
        write 0 r3 r4 ; Write word to RAM
        add r3 1 r3 ; Increment counter/dest address
        add r2 1 r2 ; Increment source address
        beq r1 r3 2 ; If done, exit loop
        jump CopyLoop

    ; Clear registers
    load 0 r1
    load 0 r2
    load 0 r3
    load 0 r4
    nop
    nop
    ccache ; Clear cache
    nop
    nop
    jump 0 ; Jump to start of RAM


Program_RAM:
    .dw 0b10010000000000000000000000000110
    .dw 0b10010000000000000000000000001000
    .dw 0b00000000000000000000000000010001
    .dw 0b11111111111111111111111111111111
    .dw 0b10110000000000000000000000010000
    .dw 0b10110000000000000000000000100000
    .dw 0b11000000000000000000000000000001
    .dw 0b00011100000000000000000100100010
    .dw 0b01100000000000000010000100100000
    .dw 0b10010000000000000000000000011100
    .dw 0b00011100000000000000000000010001 ; UART tx address
    .dw 0b00011101000001110000000000010001 ; UART tx address
    .dw 0b11100000000000000001000100000010 ; Read data from UART
    .dw 0b11010000000000000000000100100000 ; Echo back to UART
    .dw 0b10100000000000000000000000000010
    .dw 0b10100000000000000000000000000001
    .dw 0b01000000000000000000000000000000
