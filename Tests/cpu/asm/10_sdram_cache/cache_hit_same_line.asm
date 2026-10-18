; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache lines of 32 bytes. Data lives at 0x10000, away from the code.
Main:
    load32 0x10000 r10 ; data base address
    load 7 r1 ; Dummy data
    add r10 32 r2 ; byte address 0x10020 (2nd cache line)
    write 0 r2 r1 ; Write 7 to 2nd cache line with offset 0

    ; Write again to same cache line, should be a cache hit
    add r1 3 r3 ; r3=10
    write 28 r2 r3 ; Write 10 to 2nd cache line with offset 28 (last word of cache line)

    ; Now read both back
    read 32 r10 r5 ; Read 0x10020 into r5, should be 7

    read 60 r10 r6 ; Read 0x1003C into r6, should be 10

    add r5 r6 r15 ; expected=17

    halt
