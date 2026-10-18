Main:
    load32 -7 r1
    not r1 r2
    add r2 1 r3
    
    or r3 r0 r15 ; expected=7
    halt
