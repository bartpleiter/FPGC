Main:
    ; Test fadd carry propagation: low word overflow into high word
    ; f0 = {0, 0xFFFFFFFF}, f1 = {0, 1}
    ; fadd f0 f1 -> f2 = {1, 0}
    ; fsthi f2 -> r3 = 1

    load 0 r1
    load32 0xFFFFFFFF r2
    fld r1 r2 r0             ; f0 = {0, 0xFFFFFFFF}

    load 0 r1
    load 1 r2
    fld r1 r2 r1             ; f1 = {0, 1}

    fadd r0 r1 r2            ; f2 = f0 + f1 = {1, 0}

    fsthi r2 r0 r3           ; r3 = 1
    fstlo r2 r0 r4           ; r4 = 0

    add r3 r0 r15            ; expected=1

    halt
