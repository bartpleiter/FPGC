; Unaligned accesses: a word access not at a multiple of 4, and a halfword access at an
; odd address. Each one sets the unaligned-access flag (kind 2), and is done at the
; address rounded down. After each case, the program reads ERR_STATUS and ERR_PC, and
; clears them.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load32 0x1000 r13       ; data in SDRAM
    load32 0x11223344 r1
    write 0 r13 r1

    read 2 r13 r2           ; the word at 0x1000: 0x11223344
    read 8 r10 r3           ; ERR_STATUS: 0x202
    read 12 r10 r4          ; ERR_PC: the address of the read
    write 8 r10 r0

    readhu 3 r13 r2         ; the halfword at 0x1002: 0x1122
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    writeh 1 r13 r0         ; the halfword at 0x1000
    read 0 r13 r2           ; 0x11220000
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    write 3 r13 r1          ; the word at 0x1000
    read 9 r10 r15 ; expected=514 (ERR_STATUS, as the read is rounded down too)
    halt
