Main:
    load32 -8 r1
    load 7 r2

    sltu r1 r2 r15 ; expected=0
    halt
