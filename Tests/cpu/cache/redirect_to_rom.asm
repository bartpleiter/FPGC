; A redirect while the L1I fills a line that IF no longer needs, to the ROM. As in
; redirect_to_hit.asm, line 1 is filled although the jump at the end of line 0 doesn't
; need it, but this jump goes to the ROM. IF continues there at once, and the ROM's
; jump 0 comes back to the start, while the fill may still be running. IF then waits for
; the fill to end and reads the start again. The second time round, the ccached runs
; again, and Main goes to the halt. The ccached makes line 0 hit from then on, as in
; redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
Main:
    beq r1 r0 First         ; word 1, taken the first time round
    halt                    ; word 2
First:
    load 1 r1               ; word 3
    load32 0x1E000000 r2    ; words 4 and 5: the ROM
    nop
    jumpr 0 r2              ; word 7: line 1 misses behind it, and its fill starts
    nop                     ; word 8, in line 1: fetched, then flushed
