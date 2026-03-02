Main:
    ; Test fmul: 1.0 × 1.0 = 1.0 in 32.32 fixed-point
    ; 1.0 = {1, 0} = 0x0000000100000000
    ; Product 128-bit: 0x00000001_00000000 × 0x00000001_00000000 = 0x00000000_00000001_00000000_00000000
    ; Result = product[95:32] = 0x00000001_00000000 = {1, 0}

    load 1 r1
    load 0 r2
    fld r1 r2 r0             ; f0 = {1, 0} = 1.0

    load 1 r1
    load 0 r2
    fld r1 r2 r1             ; f1 = {1, 0} = 1.0

    ; fmul areg breg dreg: f[dreg] = f[areg] * f[breg]
    fmul r0 r1 r2            ; f2 = f0 * f1 = 1.0

    fsthi r2 r0 r3           ; r3 = f2[63:32] = 1
    fstlo r2 r0 r4           ; r4 = f2[31:0] = 0

    ; Verify: high=1, low=0
    ; r3 + r4 should be 1
    add r3 r4 r15            ; expected=1

    halt
