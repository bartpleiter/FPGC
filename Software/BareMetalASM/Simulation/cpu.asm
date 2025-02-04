; Contains a semi random mix of instructions to test hazards
Main:
    load 6 r6
    load 4 r4
    push r6
    multu r6 r4 r3 ; r3=24
    pop r6
    multu r6 r3 r6 ; r6=144
    

    halt
