; A read of the program's own code, which runs from SDRAM. The L1D is filled from SDRAM
; like for any other read, separately from the L1I, which has the same line. The line
; is the first one of the SDRAM, so its tag is 0, like that of a line that was never
; filled: only the valid bit tells them apart.
; runs from SDRAM
; sdram reads: 0 0
    read 4 r0 r1            ; misses in the L1D: the word of the next instruction
    add r1 1 r2
    halt
