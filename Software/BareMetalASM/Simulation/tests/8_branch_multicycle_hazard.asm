; Tests the issue of a branch evaluating to true during a stall of a multicycle instruction
Main:
    load32 0x7000000 r1 ; MU address
    read 0xFF r1 r2     ; out of bounds, should return 0
    bne r2 r0 3         ; should evaluate to false
        load 7 r15      ; expected=7
        halt
    load 6 r15
    halt
