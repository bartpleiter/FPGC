; A write to a line while the line is still being filled. The read misses, and
; continues with its word as it arrives (early restart). The write behind it is to the
; next word, which arrives while the write is in MEM. A write doesn't take a word from
; the SDRAM port: it waits for the whole line, and then writes it as a write hit, so
; the fill can't overwrite the written word. The read behind the write finds it.
; runs from SDRAM
; sdram reads: 0 800 1
; sdram writes: 800
    load32 0x10000 r1
    load 0x55 r2
    read 0 r1 r3            ; misses: word 0 arrives first
    write 4 r1 r2           ; word 1: waits for the whole line, then writes
    read 4 r1 r4            ; 0x55
    ccached                 ; writes line 800 back
    halt
