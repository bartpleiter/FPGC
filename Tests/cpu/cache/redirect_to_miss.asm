; A redirect while the L1I fills a line that IF no longer needs, to another line that
; misses. Line 1 is filled although the jump at the end of line 0 goes to line 2. IF
; waits for the fill of line 1 to end, reads its new PC again, misses, and only then
; requests line 2. The ccached makes line 0 hit from then on, as in
; redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0 1 2
    ccached                 ; word 0
    load 1 r1               ; word 1
    nop
    nop
    nop
    nop
    nop
    jump Other              ; word 7: line 1 misses behind it, and its fill starts
    load 2 r1               ; words 8 to 15, line 1: never executed
    load 3 r1
    load 4 r1
    load 5 r1
    load 6 r1
    load 7 r1
    load 8 r1
    load 9 r1
Other:
    add r1 10 r2            ; word 16, line 2
    halt
