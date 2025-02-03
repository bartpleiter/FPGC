; Simple program to test some basic CPU operations

Main:
    nop
    load 3 r1           ; r1 = 3
    load 4 r2           ; r2 = 4
    mults r1 r1 r3
    mults r1 r2 r4
    mults r4 r3 r5
    add r5 r4 r6

Int:
    reti
