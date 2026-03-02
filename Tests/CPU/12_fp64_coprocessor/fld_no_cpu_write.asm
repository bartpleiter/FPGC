Main:
    ; Test that fld does NOT write to CPU register file
    ; fld uses dreg field for FP register, but ControlUnit sets dreg_we=1 for ARITHM
    ; We must suppress this. Verify that the CPU register is not modified.

    load 99 r5               ; r5 = 99 (preserve this value)
    load 10 r1
    load 20 r2

    ; fld r1 r2 r5 -> FP: f5 = {10, 20}, but CPU r5 should remain 99
    fld r1 r2 r5

    ; If dreg_we suppression works, r5 should still be 99
    ; If it fails, r5 would be overwritten with some garbage
    add r5 r0 r15            ; expected=99

    halt
