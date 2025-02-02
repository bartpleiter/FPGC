; Simple program to test some basic CPU operations

Main:
    nop
    load 7 r1
    push r1
    pop r1
    add r1 5 r2

    nop
    load32 0x7800001 r1
    read 0 r1 r2

    halt

Int:
    reti
