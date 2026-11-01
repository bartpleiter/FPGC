; A stalled IF with a held L1I hit while the controller writes the L1I (point 7). IF
; follows the fill of line 0 as its words arrive (early restart), until CCACHE makes
; MEM wait. The word that IF waits for then arrives while IF is stalled, so IF reads it
; again once the fill is done, and holds that hit while CCACHE waits for the walk. The
; walk invalidates the L1I, so that output stops being current. CCACHE then redirects
; to the next word, and IF reads it again: it misses, and line 0 is read again.
; runs from SDRAM
; sdram reads: 0 0
    load 1 r1
    ccache                  ; IF holds a hit on an add meanwhile
    add r1 1 r2
    add r2 1 r3
    add r3 1 r4
    halt
