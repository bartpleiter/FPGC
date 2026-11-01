; CCACHED, which writes the dirty lines back and invalidates only the L1D. The write
; makes line 800 (0x10000) dirty. CCACHED writes it back, and redirects to the next
; word, which still hits in the L1I: line 0 isn't read again. The read after it misses
; in the L1D and finds the written word in SDRAM. It continues with that word as it
; arrives (early restart), and the halt retires before the controller gets to line 1,
; the code behind the halt, which IF fetches ahead.
; runs from SDRAM
; sdram reads: 0 800 800
; sdram writes: 800
    load32 0x10000 r1
    load32 0xCAFE r2
    write 4 r1 r2           ; misses: line 800, then dirty
    ccached                 ; writes back 800, then continues at the next word
    read 4 r1 r3            ; misses: 0xCAFE, from the write-back
    halt
