Main:
    load 7 r1
    load 4 r2
    
    shiftl r1 16 r1
    shiftl r2 16 r2

    multfp r1 r2 r3

    shiftr r3 16 r15 ; expected=28
    
    halt
