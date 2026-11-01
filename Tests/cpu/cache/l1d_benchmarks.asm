; The micro-benchmarks of the L1D (test_l1d_micro_benchmarks), with B32P3's SDRAM
; timing. The loop runs twice, and the second time the code hits in the L1I, so only
; the L1D makes MEM wait. Its first read is a clean miss: the line it replaces was only
; read. It continues with its word as soon as that arrives (early restart), and so does
; the read of the next word, which arrives in the cycle after. The nops give the rest of
; the line time to arrive. Then a read hit, a write hit, a read right behind a write,
; and a read with a dirty line to write back first. Each iteration uses the next two
; lines with index 0.
; runs from SDRAM
; sdram reads: 0 800 1 2 880 900 980
; sdram writes: 800 900
    load32 0x10000 r1
    load 0x2000 r7
    load 2 r8               ; the iterations
    load 5 r2
Loop:
    nop                     ; after the redirect, so the miss behind it waits alone
    read 0 r1 r3            ; a clean miss
    read 4 r1 r4            ; the next word, as it arrives
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    read 12 r1 r4           ; a read hit
    write 8 r1 r2           ; a write hit
    read 8 r1 r5            ; a read right behind a write
    read 0x1000 r1 r6       ; a miss with a dirty line
    add r1 r7 r1
    sub r8 1 r8
    bne r8 r0 Loop
    halt
