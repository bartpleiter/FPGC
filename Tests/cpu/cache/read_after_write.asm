; A read right behind a write to the same word. The read waits a cycle in EX for the
; write (the write-then-access stall), so the L1D has the new word and the dirty bit
; when it reads them. First while the write misses, then while it hits, then with a
; halfword written into a word. Line 1 is the code of the last two instructions.
; runs from SDRAM
; sdram reads: 0 800 1
    load32 0x10000 r1
    load 0x1234 r2
    write 8 r1 r2           ; misses, then writes
    read 8 r1 r3            ; right behind it: 0x1234
    add r2 r2 r2
    write 8 r1 r2           ; hits
    read 8 r1 r4            ; right behind it: 0x2468
    writeh 10 r1 r2         ; hits, the upper half
    read 8 r1 r5            ; right behind it: 0x24682468
    halt
