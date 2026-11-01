; A word that arrives while IF is stalled is lost. IF follows the fill of line 1 as its
; words arrive (early restart), until the add behind the multiply waits a cycle in ID
; for its result (the late-result stall). The word that arrives in that cycle isn't
; used, so IF waits for the fill to end, and reads the word from the L1I.
; runs from SDRAM
; sdram reads: 0 1
    load 3 r1               ; word 0
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    load 2 r2               ; word 8, line 1
    mults r1 r2 r3
    add r3 1 r4             ; waits a cycle in ID
    add r4 1 r5             ; word 11: its word arrives while IF is stalled
    add r5 1 r6
    halt
