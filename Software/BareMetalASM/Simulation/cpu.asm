; Simple program to test some basic CPU operations

Main:
    load 7 r1
    add r1 1 r1
    nop
    add r1 2 r1
    nop
    nop
    nop
    halt
    push r1
    nop
    nop
    nop
    load32 0xFFFFFFFF r1
    pop r1
    nop
    nop
    nop
    add r1 9 r2
    halt

Int:
    reti
