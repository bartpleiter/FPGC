Main:
    load32 100100 r1
    push r1
    pop r2
    or r1 r0 r15 ; expected=100100
    halt

