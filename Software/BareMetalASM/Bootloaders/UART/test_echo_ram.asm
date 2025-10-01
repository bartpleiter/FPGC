Main:
    load32 0x7000000 r1 ; UART tx address
    load 65 r2        ; ASCII 'A'
    write 0 r1 r2     ; Send 'A' to UART
    halt

Int:
    ; Backup regs
    push r1
    push r2

    ; Check interrupt ID to be 1 (UART RX)
    readintid r1
    load 1 r2
    beq r1 r2 2
    jump ReturnInt

    ; Read char from UART
    load32 0x7000000 r1 ; UART tx address
    read 1 r1 r2        ; Read data from UART
    write 0 r1 r2       ; Echo back to UART

    ReturnInt:
        pop r2
        pop r1
        reti
