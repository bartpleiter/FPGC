Main:
    load32 -8 r1
    shiftrs r1 2 r2 ; -2

    load 16 r1
    shiftrs r1 1 r3 ; 8

    add r2 r3 r15 ; expected=6
    halt
