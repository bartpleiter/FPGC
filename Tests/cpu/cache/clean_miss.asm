; A clean L1I miss in straight-line code: the code runs from line 0 into line 1, which
; misses. It's also the micro-benchmark of the clean miss (test_clean_l1i_miss): with
; B32P3's SDRAM timing, IF waits at most 8 cycles for the first word of line 1, which
; comes first and goes to ID in the cycle it arrives (early restart). The ccached first
; makes the pipeline wait until line 0 is filled, so that line 0 hits from then on, and
; the miss of line 1 comes on its own: without it, IF would follow the fill of line 0
; word by word, and reach line 1 only as that fill ends (follow_fill.asm).
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
    load 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1             ; word 7
    add r1 1 r1             ; word 8, the first word of line 1
    halt
