; A loop across a line boundary of the code, which reads and writes two lines of data
; with the same index in every iteration (0x10000 and 0x11000, also with 4 lines). Each
; access to the other line writes the dirty one back and fills the other, so every
; iteration makes two write-backs and two fills, while the loop itself hits in the L1I
; from the second iteration on. IF follows the fill of line 0 as its words arrive
; (early restart), and reaches line 1 only after that fill, when the first read has
; already missed. Line 2 is the code behind the halt.
; runs from SDRAM
; sdram reads: 0 800 1 880 800 880 800 880 2
; sdram writes: 800 880 800 880 800
    load32 0x10000 r1
    load 3 r2               ; the iterations
    nop
    nop
    nop
Loop:
    read 0 r1 r3            ; word 6, line 0
    add r3 1 r3
    write 0 r1 r3           ; line 800 is dirty
    read 0x1000 r1 r4       ; word 9, line 1: writes line 800 back, fills line 880
    add r4 r3 r4
    write 0x1000 r1 r4      ; line 880 is dirty
    sub r2 1 r2
    bne r2 r0 Loop
    halt
