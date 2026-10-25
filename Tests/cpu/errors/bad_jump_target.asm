; Bad jump targets: a jump or taken branch to an address that isn't a multiple of 4.
; Each one sets the bad-jump-target flag (kind 4), and jumps to the address rounded
; down. A target outside SDRAM and ROM is bad too, but the ISS stops when it fetches
; from there. After each case, the program reads ERR_STATUS and ERR_PC, and clears
; them.
Main:
    load32 0x1F000000 r10   ; the CPU-internal I/O registers
    addr2reg Target r1
    jumpr 2 r1              ; to Target + 2, so to Target
    load 1 r15              ; skipped
Target:
    read 8 r10 r3           ; ERR_STATUS: 0x408
    read 12 r10 r4          ; ERR_PC: the address of the jumpr
    write 8 r10 r0

    .dw 0x90000015          ; jumpo 10, so the next instruction is skipped
    load 1 r15
    read 8 r10 r3
    read 12 r10 r4
    write 8 r10 r0

    .dw 0x6000A000          ; beq r0 r0 10, taken
    load 1 r15
    read 8 r10 r15 ; expected=1032
    halt
