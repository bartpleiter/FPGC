Main:
    ; Test that FADD self-modify works correctly even with potential pipeline stalls
    ; This exercises the fp_write_pending mechanism:
    ; f0 is written once per instruction even if the pipeline stalls
    ;
    ; f0 = {0, 10}, then fadd f0 f0 f0 = {0, 20}
    ; Verify we get exactly 20, not some higher power-of-2 from repeated writes

    load 0 r1
    load 10 r2
    fld r1 r2 r0             ; f0 = {0, 10}

    ; Self-modify: f0 = f0 + f0 = {0, 20}
    fadd r0 r0 r0

    ; Do it again: f0 = f0 + f0 = {0, 40}
    fadd r0 r0 r0

    ; Extract
    fstlo r0 r0 r15          ; expected=40

    halt
