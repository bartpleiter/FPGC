; Simple program to test some basic CPU operations

Main:
    nop
    load 8 r1
    load 3 r2
    mults r1 r2 r3
    add r3 1 r3
    mults r3 r2 r1
    halt

Int:
    reti
