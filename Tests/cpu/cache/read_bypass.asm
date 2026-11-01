; Reads that continue with their word as it arrives from SDRAM (early restart). The
; data are the words of lines 2 and 3 of the program. The first read misses at word 2
; of line 2, so its fill brings words 2 to 7, then 0 and 1, and the read continues
; with its word as it arrives. The read behind it wants word 3, which arrives in the
; next cycle, and takes it from the SDRAM port too. The read after the nop wants word
; 4, which has passed by then, so it waits for the fill to end and reads the L1D. The
; read of line 3 misses at word 7, and the read behind it wants word 0, which comes
; next as the fill wraps around.
; runs from SDRAM
; sdram reads: 0 2 1 3
    addr2reg Data r1
    read 8 r1 r2            ; word 2 of line 2: misses
    read 12 r1 r3           ; word 3: arrives in the cycle after
    nop
    read 16 r1 r4           ; word 4: has passed, so it waits and reads the L1D
    read 0x3C r1 r5         ; word 7 of line 3: misses
    read 0x20 r1 r6         ; word 0 of line 3: arrives in the cycle after
    halt
    nop
    nop
    nop
    nop
    nop
    nop
    nop
Data:                       ; line 2 (0x40)
    .dw 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27
    .dw 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37  ; line 3 (0x60)
