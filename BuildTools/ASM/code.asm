; Simple program to test some basic CPU operations

Main:
    nop
    load 3 r1
    load 4 r2
    mults r1 r2 r3
    add r3 r2 r4
    nop
    nop
    nop
    halt
        

Int:
    reti
