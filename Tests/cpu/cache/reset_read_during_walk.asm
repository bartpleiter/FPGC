; A reset in the middle of the run, and a read of SDRAM right after it, from the ROM,
; while the walk after the reset still invalidates the L1D. The reset comes in the
; delay loop, and the program then runs again from the start. Before the reset, the
; line of 0xFE0 is dirty in the L1D with 0x5A5A, and its index is the last one, so the
; walk reaches it last. The read after the reset must still give 0 from SDRAM: MEM may
; not use the L1D until the walk is done, and the walk doesn't write the line back.
; reset at cycle 1000
    load 0xFE0 r1
    read 0 r1 r2            ; 0, also after the reset
    load 0x5A5A r3
    write 0 r1 r3           ; a dirty line, which is never written back
    load 300 r4
Delay:                      ; the reset comes in this loop
    sub r4 1 r4
    bne r4 r0 Delay
    halt
