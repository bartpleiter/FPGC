; A redirect while the L1I fills a line that IF no longer needs, to the ROM. As in
; redirect_to_hit.asm, line 1 is filled although the jump at the end of line 0 doesn't
; need it, but this jump goes to the ROM: to the halt behind the ROM's jump to SDRAM.
; IF continues there at once, while the fill may still be running. The ccached makes
; line 0 hit from then on, as in redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
    nop
    nop
    nop
    load32 0x04000004 r2    ; words 4 and 5: the halt in the ROM
    nop
    jumpr 0 r2              ; word 7: line 1 misses behind it, and its fill starts
    nop                     ; word 8, in line 1: fetched, then flushed
