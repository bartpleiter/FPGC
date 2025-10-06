; Simple program to test some SPI functionality
Main:


    load32 0x7000000 r1 ; MU base address
    load 0x90 r2 ; Read ID command
    load 1 r3 ; Value one

    nop
    nop
    nop

    ; Set CS low
    write 13 r1 r0
    nop
    nop
    nop

    ; Send command
    write 12 r1 r2
    nop
    nop
    nop

    ; Read response
    write 12 r1 r0
    nop
    nop
    nop
    read 12 r1 r4 ; Read ID byte
    nop
    nop
    nop


    ; Set CS high
    write 13 r1 r3
    nop
    nop
    nop
    ; Send bytes over UART
    write 0 r1 r4

    halt




    ; load32 0x7000000 r1 ; MU base address
    ; load 0x9F r2 ; Read ID command
    ; load 1 r3 ; Value one

    ; nop

    ; ; Set CS low
    ; write 9 r1 r0
    ; nop
    ; nop
    ; nop

    ; ; Send command
    ; write 8 r1 r2
    ; nop
    ; nop
    ; nop

    ; ; Read response
    ; write 8 r1 r0
    ; nop
    ; nop
    ; nop
    ; read 8 r1 r4 ; Read ID byte
    ; nop
    ; nop
    ; nop

    ; write 8 r1 r0
    ; nop
    ; nop
    ; nop
    ; read 8 r1 r5 ; Read Memory type
    ; nop
    ; nop
    ; nop

    ; write 8 r1 r0
    ; nop
    ; nop
    ; nop
    ; read 8 r1 r6 ; Read Capacity
    ; nop
    ; nop
    ; nop

    ; ; Set CS high
    ; write 9 r1 r3
    ; nop

    ; ; Send bytes over UART
    ; write 0 r1 r4
    ; write 0 r1 r5
    ; write 0 r1 r6

    ; halt


; Ignore interrupts
Int:
    reti








    
