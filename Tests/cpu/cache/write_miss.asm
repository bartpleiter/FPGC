; Write misses in the L1D. A write that misses fills the line first, and then writes it
; as a write hit (write-allocate), so the bytes it doesn't write keep their value from
; SDRAM. The first write fills line 800 (0x10000) and makes it dirty, the read of line
; 880 writes it back, and the byte write misses on line 800 again: the line comes back
; from SDRAM with the word of the first write, and the byte goes into it. Line 1 is the
; code behind the halt, which IF fetches ahead.
; runs from SDRAM
; sdram reads: 0 800 880 1 800
; sdram writes: 800
    load32 0x10000 r1
    load32 0x12345678 r2
    write 0 r1 r2           ; misses: fills line 800, then writes 0x12345678
    read 0x1000 r1 r3       ; misses: writes line 800 back, then fills line 880
    writeb 1 r1 r2          ; misses: fills line 800, then writes the byte 0x78
    read 0 r1 r4            ; 0x12347878
    halt
