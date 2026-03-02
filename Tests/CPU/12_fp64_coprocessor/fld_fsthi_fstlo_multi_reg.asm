Main:
    ; Test fld/fsthi/fstlo with multiple FP registers
    ; Load different values into f0, f1, f2 and verify they don't interfere

    ; f0 = {10, 20}
    load 10 r1
    load 20 r2
    fld r1 r2 r0             ; f0 (dreg=r0, fp_dreg=0)

    ; f1 = {30, 40}
    load 30 r1
    load 40 r2
    fld r1 r2 r1             ; f1 (dreg=r1, fp_dreg=1)

    ; f2 = {50, 60}
    load 50 r1
    load 60 r2
    fld r1 r2 r2             ; f2 (dreg=r2, fp_dreg=2)

    ; Read back f0 high: should be 10
    fsthi r0 r0 r3           ; r3 = f0[63:32] = 10

    ; Read back f1 low: should be 40
    fstlo r1 r0 r4           ; r4 = f1[31:0] = 40

    ; Read back f2 high: should be 50
    fsthi r2 r0 r5           ; r5 = f2[63:32] = 50

    ; Verify: r3 + r4 + r5 = 10 + 40 + 50 = 100
    add r3 r4 r6
    add r6 r5 r15            ; expected=100

    halt
