Main:
    load32 -8 r1
    load 7 r2

    slt r1 r2 r15 ; expected=1
    halt
