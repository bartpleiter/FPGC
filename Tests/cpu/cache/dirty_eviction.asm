; A dirty conflict eviction in the L1D. The write makes line 800 (0x10000) dirty. The
; read of 0x11000, which has the same index, also with 4 lines, first writes line 800
; back, then fills line 880. The last read fills line 800 again, from SDRAM, so it only
; sees the written word if the write-back happened. Line 1 is the code behind the halt,
; which IF fetches ahead.
; runs from SDRAM
; sdram reads: 0 800 880 1 800
; sdram writes: 800
    load32 0x10000 r1
    load32 0x12345678 r2
    write 4 r1 r2           ; misses, then writes: line 800 is dirty
    read 0x1000 r1 r3       ; misses: writes line 800 back, then fills line 880
    read 4 r1 r4            ; misses: line 800 again, with 0x12345678
    halt
