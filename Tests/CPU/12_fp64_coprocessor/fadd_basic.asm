Main:
    ; Test fadd: 64-bit addition within FP registers
    ; f0 = {0, 5}, f1 = {0, 3}
    ; fadd f0 f1 -> f2 = {0, 8}
    ; fstlo f2 -> r3 = 8

    load 0 r1
    load 5 r2
    fld r1 r2 r0             ; f0 = {0, 5}

    load 0 r1
    load 3 r2
    fld r1 r2 r1             ; f1 = {0, 3}

    ; fadd areg breg dreg: f[dreg] = f[areg] + f[breg]
    ; fadd r0 r1 r2 -> f0 + f1 -> f2 = {0, 8}
    fadd r0 r1 r2

    ; Extract low word of f2
    fstlo r2 r0 r3           ; r3 = f2[31:0] = 8
    fsthi r2 r0 r4           ; r4 = f2[63:32] = 0

    add r3 r4 r15            ; expected=8

    halt
