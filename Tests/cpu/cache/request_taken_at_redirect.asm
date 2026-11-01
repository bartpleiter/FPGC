; A redirect at the edge where the controller takes IF's request. The jump at word 6 of
; line 0 redirects one cycle after IF has requested line 1, and the controller takes
; the request at the same edge. So line 1 is filled although IF no longer needs it.
; Main was read before the fill writes anything, so IF uses it at once. The next word
; is read while the controller writes the L1I, so IF waits for the fill to end and
; reads it again. The second time round, Main goes to the halt. The ccached makes line
; 0 hit from then on, as in redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0 1
    ccached                 ; word 0
Main:
    beq r1 r0 First         ; word 1, taken the first time round
    halt                    ; word 2
First:
    load 1 r1               ; word 3
    nop
    nop
    jump Main               ; word 6: redirects one cycle after the request of word 8
    nop
