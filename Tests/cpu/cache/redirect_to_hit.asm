; A redirect while the L1I fills a line that IF no longer needs, to a line that hits.
; The jump at the end of line 0 goes back to Main. IF fetches the first word of line 1
; behind it, misses, and the controller takes the request before the jump redirects.
; So line 1 is filled although IF no longer needs it. IF waits for that fill to end,
; then reads Main again, and it hits. The second time round, Main goes to the halt.
; The ccached first makes the pipeline wait until line 0 is filled, so that line 0 hits
; from then on: without it, IF would follow the fill of line 0 word by word (early
; restart), and reach line 1 only after that fill.
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
    load 0 r1               ; word 1
Main:
    beq r1 r0 First         ; word 2, taken the first time round
    halt                    ; word 3
First:
    load 1 r1               ; word 4
    nop
    nop
    jump Main               ; word 7: line 1 misses behind it, and its fill starts
    nop                     ; word 8, in line 1: fetched, then flushed
