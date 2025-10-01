Main:

    load32 0x7000000 r1 ; UART tx address
    load 250 r2        ; UART data to send

    write 0 r1 r2       ; Send data to UART
    read 1 r1 r3        ; Read data from UART

    nop
    nop
    nop
    halt
