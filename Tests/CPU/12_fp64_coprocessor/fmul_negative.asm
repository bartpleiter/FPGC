Main:
    ; Test fmul with negative: (-1.0) × 2.0 = -2.0 in signed 32.32 fixed-point
    ; -1.0 in 32.32 = 0xFFFFFFFF_00000000 (two's complement)
    ; 2.0 in 32.32 = 0x00000002_00000000
    ; Result: -2.0 = 0xFFFFFFFE_00000000
    ; fsthi should give 0xFFFFFFFE = -2 in signed

    ; Load -1.0 into f0
    load32 0xFFFFFFFF r1
    load 0 r2
    fld r1 r2 r0             ; f0 = {0xFFFFFFFF, 0} = -1.0

    ; Load 2.0 into f1
    load 2 r1
    load 0 r2
    fld r1 r2 r1             ; f1 = {2, 0} = 2.0

    ; fmul: f2 = f0 * f1 = -1.0 * 2.0 = -2.0
    fmul r0 r1 r2

    fsthi r2 r0 r3           ; r3 = f2[63:32] = 0xFFFFFFFE = -2 signed
    fstlo r2 r0 r4           ; r4 = f2[31:0] = 0

    ; Use negate: -(-2) = 2
    sub r0 r3 r5             ; r5 = 0 - (-2) = 2
    add r5 r4 r15            ; expected=2

    halt
