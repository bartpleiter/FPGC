Main:
    load 1 r1
    load 7 r1
    load 4 r2
    add r1 r2 r3 ; 11
    sub r3 1 r4 ; 10
    or r4 r1 r5 ; 15
    and r5 r2 r6 ; 4
    xor r6 r1 r7 ; 3
    shiftl r7 1 r8 ; 6
    shiftr r8 2 r9 ; 1
    
    or r9 r0 r15 ; expected=1
    halt
