; Simple program to test some basic CPU operations

Main:
    nop
    load 8 r1
    add r1 5 r2
    jumpo 2
    halt
    push r2
    load 1 r2
    pop r2
    add r2 5 r2
    add r2 6 r2

    halt

Int:
    reti
