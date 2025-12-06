; Simple program to test some SPI functionality
Main:

    load32 0x7000000 r1 ; MU base address
    load 0x9F r2 ; Read ID command
    load 1 r3 ; Value one
    
    write 9 r1 r0 ; Set CS low

    write 8 r1 r2 ; Send command
    
    write 8 r1 r0 ; Write 0 -> Read response
    read 8 r1 r4 ; Read ID byte

    write 8 r1 r0 ; Write 0 -> Read response
    read 8 r1 r5 ; Read Memory type

    write 8 r1 r0 ; Write 0 -> Read response
    read 8 r1 r6 ; Read Capacity

    write 9 r1 r3 ; Set CS high

    ; Send bytes over UART
    write 0 r1 r4
    write 0 r1 r5
    write 0 r1 r6

    halt


; Ignore interrupts
Int:
    reti
