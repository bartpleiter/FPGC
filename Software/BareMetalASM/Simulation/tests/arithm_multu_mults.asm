Main:
    load 7 r1
    load 4 r2
    multu r1 r2 r3 ; 28
    load32 -3 r2
    load 6 r1
    mults r1 r2 r4 ; -18

    add r3 r4 r15 ; expected=10
    
    halt
