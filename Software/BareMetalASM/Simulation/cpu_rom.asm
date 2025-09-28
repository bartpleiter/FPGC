; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache line size of 8 words (32 bytes), meaning that address 8 is on the 2nd cache line
Main:
    ; Semi random read/writes with the goal to create difficult situations

    load32 0xABCD r1
    load32 0x1234 r2
    load32 0x5678 r3
    write 0 r0 r1
    write 16 r0 r2
    write 24 r0 r3

    nop
    nop
    nop
    ccache
    nop
    nop

    halt
