; Bad addresses: a read or write at an unmapped address, a write to ROM, and accesses
; to the CPU-internal I/O registers that aren't a word access to a register. Each one
; sets the bad-address flag (kind 3). A bad read gives 0, and a bad write does nothing.
; After each case, the program reads ERR_STATUS and ERR_PC, and clears them.
;
; The unmapped address and the ROM address have the same low bits as a word in SDRAM
; that holds 5, so a read or write that ignores the region would show up there.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    load32 0x10001000 r12   ; unmapped, between SDRAM and I/O
    load32 0x1E000000 r13   ; ROM
    load32 0x1000 r11       ; SDRAM, with the same bits [25:2] as r12
    load32 0x2000000 r9     ; SDRAM, with the same bits [25:2] as r13
    load 5 r1
    write 0 r11 r1
    write 0 r9 r1

    read 0 r12 r2           ; 0, not the 5 in SDRAM
    read 8 r10 r3           ; ERR_STATUS: 0x304
    read 12 r10 r4          ; ERR_PC: the address of the read
    write 8 r10 r0

    write 0 r12 r0
    read 0 r11 r2           ; still 5
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    write 0 r13 r0          ; ROM can't be written
    read 0 r9 r2            ; still 5
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    readb 4 r10 r2          ; a byte of SP: 0
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    write 12 r10 r1         ; ERR_PC can't be written
    read 8 r10 r3
    read 12 r10 r4          ; the address of the write, not 5
    write 8 r10 r0

    read 2 r10 r2           ; unaligned (kind 2), reads PC_BACKUP
    writeh 8 r10 r0         ; a halfword write, so it doesn't clear ERR_STATUS
    read 8 r10 r15 ; expected=518 (0x206)
    halt
