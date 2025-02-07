; Contains a semi random mix of instructions to test hazards
Main:
    load 3 r3 ; r3=3
    load 4 r4 ; r4=4

    load 1 r1 ; r1=1
    load 2 r2 ; r2=2
    add r1 r2 r5 ; r5=3
    beq r3 r5 2
    halt

    multu r2 r2 r6 ; r6=4
    beq r4 r6 2
    halt
    add r1 r6 r6 ; r6=5
    multu r6 r2 r6 ; r6=10
    multu r6 r2 r6 ; r6=20
    sub r6 r2 r6 ; r6=18
    multu r6 r2 r1 ; r1=36
    load 35 r2 ; r2=35

    sub r1 r2 r6 ; r6=1
    push r6
    multu r6 r4 r3 ; r3=4
    pop r6
    multu r6 r3 r6 ; r6=4
    push r6
    load 9 r6 ; r6=9
    pop r6
    add r6 2 r6 ; r6=6
    bge r6 r3 2
    halt
    or r6 r0 r15 ; expected=6

    halt
