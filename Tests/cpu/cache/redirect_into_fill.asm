; A redirect while the L1I fills a line that IF no longer needs at that word, into the
; line being filled. The jump at the end of line 0 goes to word 3 of line 1, while line
; 1 is filled from word 0 on. The tag of line 1 is only written with the last word, so
; IF waits for the fill to end, reads its new PC again, and hits. Line 1 is read from
; SDRAM once. The ccached makes line 0 hit from then on, as in redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
    load 1 r1               ; word 1
    nop
    nop
    nop
    nop
    nop
    jump Inside             ; word 7: line 1 misses behind it, and its fill starts
    load 2 r1               ; words 8 to 10, line 1: skipped
    load 3 r1
    load 4 r1
Inside:
    add r1 10 r2            ; word 11
    halt
