; Simple program to test some basic CPU operations

Main:
    nop
    load 7 r1
    load 9 r2
    nop
    mults r1 r2 r3
    nop
    nop
    multu r1 r2 r4
    nop
    nop
    shiftl r1 16 r1
    shiftl r2 16 r2
    multfp r1 r2 r5
    halt
        

Int:
    reti
