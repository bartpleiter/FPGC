; CCACHE with dirty lines, which are then read back. The writes make lines 800 and 801
; (0x10000 and 0x10020) dirty, and the read keeps line 882 (0x11040) clean in the L1D.
; CCACHE writes the two dirty lines back, in the order of their index, and invalidates
; both caches. It then redirects to the next word, so IF fetches the code again, from
; line 0, and every read after it misses and fills its line from SDRAM, where it finds
; the words that were written back. The second time, the halt retires before the
; controller gets to line 1, the code behind it, which IF fetches ahead: the reads
; continue with their words as they arrive (early restart).
; runs from SDRAM
; sdram reads: 0 800 801 882 1 0 800 801 882
; sdram writes: 800 801
    load32 0x10000 r1
    load32 0xCAFE r2
    write 4 r1 r2           ; misses: line 800, then dirty
    write 0x24 r1 r2        ; misses: line 801, then dirty
    read 0x1040 r1 r3       ; misses: line 882, clean
    ccache                  ; writes back 800 and 801, then continues at the next word
    read 4 r1 r4            ; misses: 0xCAFE, from the write-back
    read 0x24 r1 r5         ; misses: 0xCAFE
    read 0x1040 r1 r6       ; misses: line 882 again
    halt
