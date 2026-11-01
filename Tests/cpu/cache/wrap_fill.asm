; A fill that starts in the middle of a line and wraps around, with a branch back into
; it. The jump goes to word 5 of line 1, so its fill brings words 5, 6, 7, 0, 1, 2, 3
; and 4, in that order, and IF follows words 5 to 7 as they arrive (early restart). The
; jump at word 7 goes back to word 4, which arrives last: IF takes it from the SDRAM
; port too. Word 5 has passed by then, so IF waits for the fill to end and reads it from
; the L1I. The second time round, the jump at word 7 redirects while line 2 is filled
; behind it, and the third time the branch goes to the halt in line 2.
; runs from SDRAM
; sdram reads: 0 1 2
    load 2 r1               ; word 0
    jump Inside             ; word 1
    nop
    nop
    nop
    nop
    nop
    nop
    nop                     ; word 8, line 1
    nop
    nop
    nop
Back:
    sub r1 1 r1             ; word 12, word 4 of line 1
Inside:
    beq r1 r0 Done          ; word 13, word 5 of line 1
    nop
    jump Back               ; word 15
Done:
    halt                    ; word 16, line 2
