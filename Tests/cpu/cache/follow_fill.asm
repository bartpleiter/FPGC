; Straight-line code that follows the fills of its lines (early restart). Each line
; misses in the L1I, and its fill starts at the word IF waits for, the first one of the
; line. IF passes each word to ID in the cycle it arrives, so with a word per cycle the
; instructions of a line retire one per cycle. IF reaches the next line as the fill
; ends, reads it once the controller has stopped writing the L1I, and misses again.
; runs from SDRAM
; sdram reads: 0 1 2
    load 1 r1               ; word 0
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1             ; word 7
    add r1 1 r1             ; word 8, line 1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1
    add r1 1 r1             ; word 15
    add r1 1 r1             ; word 16, line 2
    halt
