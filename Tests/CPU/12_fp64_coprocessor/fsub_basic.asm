Main:
    ; Test fsub: 64-bit subtraction within FP registers
    ; f0 = {0, 10}, f1 = {0, 3}
    ; fsub f0 f1 -> f2 = {0, 7}

    load 0 r1
    load 10 r2
    fld r1 r2 r0             ; f0 = {0, 10}

    load 0 r1
    load 3 r2
    fld r1 r2 r1             ; f1 = {0, 3}

    fsub r0 r1 r2            ; f2 = f0 - f1 = {0, 7}

    fstlo r2 r0 r3           ; r3 = 7

    add r3 r0 r15            ; expected=7

    halt
