Main:
    ; Test fmul: 2.5 × 2.0 = 5.0 in 32.32 fixed-point
    ; 2.5 = {2, 0x80000000}  (2 + 0.5, where 0.5 = 0x80000000 in fractional part)
    ; 2.0 = {2, 0}
    ; Product = 5.0 = {5, 0}

    ; Load 2.5 into f0
    load 2 r1
    load32 0x80000000 r2
    fld r1 r2 r0             ; f0 = {2, 0x80000000} = 2.5

    ; Load 2.0 into f1
    load 2 r1
    load 0 r2
    fld r1 r2 r1             ; f1 = {2, 0} = 2.0

    ; fmul: f2 = f0 * f1 = 2.5 * 2.0 = 5.0
    fmul r0 r1 r2

    fsthi r2 r0 r3           ; r3 = f2[63:32] = 5
    fstlo r2 r0 r4           ; r4 = f2[31:0] = 0

    ; Verify high=5, low=0
    add r3 r4 r15            ; expected=5

    halt
