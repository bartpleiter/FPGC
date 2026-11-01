; A clean conflict eviction in the L1D. 0x10000 and 0x11000 have the same index, also
; with 4 lines. The second read replaces the clean line of the first without writing it
; back, and the third read fills the first line again.
; runs from SDRAM
; sdram reads: 0 800 880 800
    load32 0x10000 r1
    read 0 r1 r2            ; misses: line 800
    read 0x1000 r1 r3       ; misses: line 880 replaces it, nothing is written back
    read 4 r1 r4            ; misses: line 800 again
    halt
