; Illegal instructions: a reserved ALU operation, branch condition, read size and write
; size. Each one sets the illegal-instruction flag (kind 1), and does nothing else.
; After each case, the program reads ERR_STATUS and ERR_PC, and clears them.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load32 0x1000 r13       ; data in SDRAM
    load 7 r1
    write 0 r13 r0

    .dw 0x08000123          ; reserved ALU operation 1000 on r1 and r2, into r3
    read 8 r10 r4           ; ERR_STATUS: 0x101
    read 12 r10 r5          ; ERR_PC: the address of the .dw
    write 8 r10 r0

    .dw 0x60008016          ; a branch with reserved condition 011 to +8: not taken
    load 1 r3
    read 8 r10 r4
    read 12 r10 r5
    write 8 r10 r0

    .dw 0xE0000D36          ; a read with reserved size 0011 at r13, into r6
    read 8 r10 r4
    read 12 r10 r5
    write 8 r10 r0

    .dw 0xD0000D13          ; a write with reserved size 0011 of r1 at r13
    read 0 r13 r6           ; still 0
    read 8 r10 r4
    read 12 r10 r5
    write 8 r10 r0

    .dw 0x08000123
    read 8 r10 r15 ; expected=257
    halt
