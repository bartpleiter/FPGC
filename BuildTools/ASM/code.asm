; Simple program to test some basic CPU operations

Main:
    nop
    load 3 r1
    add r1 1 r1
    push r1
    load32 2 r1
    pop r1
    add r1 9 r2
    halt

Int:
    reti
