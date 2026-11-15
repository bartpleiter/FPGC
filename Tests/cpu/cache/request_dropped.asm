; A redirect drops a request that the controller hasn't taken. The jump at word 5 of
; line 0 redirects in the cycle in which IF sees the miss of line 1, so the request for
; line 1 is never made, and line 1 is never read. The second time round, Main goes to
; the halt before IF reaches line 1. The ccached makes line 0 hit from then on, as in
; redirect_to_hit.asm.
; runs from SDRAM
; sdram reads: 0
    ccached                 ; word 0
    load 0 r1               ; word 1
Main:
    beq r1 r0 First         ; word 2, taken the first time round
    halt                    ; word 3
First:
    load 1 r1               ; word 4
    jump Main               ; word 5: redirects when IF sees the miss of word 8
    nop
    nop
