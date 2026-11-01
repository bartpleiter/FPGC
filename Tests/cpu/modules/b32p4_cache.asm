; The caches in the pipeline (fpgc cpu wave b32p4_cache).
; The program runs from SDRAM. After the jump from the ROM, IF first waits for the cache
; controller's walk after reset, which invalidates both caches, one line per cycle.
; Then the first fetch misses in the L1I: IF waits while the controller fills the line.
; The words arrive from the one IF waits for, and IF passes each one on in the cycle it
; arrives (early restart). The first read misses in the L1D, and MEM waits for its word
; in the same way, while the controller fills the line. The write behind it is to the
; same line, which is still arriving. A write waits for the whole line, then reads it
; again, hits, and makes the line dirty. The last read is of another line with the same
; index, so the controller first writes the dirty line back, then fills the new one, and
; the read continues with its word as it arrives.
; runs from SDRAM
load32 0x10000 r1
load 7 r2
read 0 r1 r3        ; misses: MEM continues with the word as it arrives: 0
write 4 r1 r2       ; waits for the whole line, then hits: writes 7, and the line is dirty
read 0x1000 r1 r4   ; waits a cycle in EX for the write, then misses: write-back, fill
add r3 r4 r5        ; waits a cycle in ID for the read
halt
