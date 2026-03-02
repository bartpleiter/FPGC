Main:
    ; Test fmul: 0.5 × 0.5 = 0.25 in 32.32 fixed-point
    ; 0.5 = {0, 0x80000000}
    ; 0.25 = {0, 0x40000000}
    ; Verify by extracting the fractional part

    ; Load 0.5 into f0 and f1
    load 0 r1
    load32 0x80000000 r2
    fld r1 r2 r0             ; f0 = {0, 0x80000000} = 0.5
    fld r1 r2 r1             ; f1 = {0, 0x80000000} = 0.5

    ; fmul: f2 = f0 * f1 = 0.5 * 0.5 = 0.25
    fmul r0 r1 r2

    fsthi r2 r0 r3           ; r3 = f2[63:32] = 0 (integer part)
    fstlo r2 r0 r4           ; r4 = f2[31:0] = 0x40000000 (fractional part)

    ; Verify: shift right by 30 to get 1 (0x40000000 >> 30 = 1)
    shiftr r4 30 r15         ; expected=1

    halt
