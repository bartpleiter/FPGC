; Reads and writes outside SDRAM (fpgc cpu wave b32p4_bus).
; They go over the bus. MEM sends a request for one cycle and waits until the answer
; comes; the word of a read goes into a register first, and MEM uses it in the cycle
; after that. The read of the ROM goes over the bus too, as data, and the add behind
; the last read waits until that read has its word.
    addr2reg Table r1
    load32 0xFFFF8000 r2    ; the I/O region
    read 0 r1 r3            ; 0x12345678, from the ROM
    write 0 r2 r3           ; to the I/O region
    read 0 r2 r4            ; 0x12345678, back from the I/O region
    add r4 1 r5             ; 0x12345679
    halt
Table:
    .dw 0x12345678
