; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache line size of 8 words (32 bytes), meaning that address 8 is on the 2nd cache line
Main:
    load 7 r1 ; Dummy data
    load 8 r2 ; SDRAM address 8 (2nd cache line)
    write 0 r2 r1 ; Write 7 to 2nd cache line with offset 0

    ; Write again to same cache line, should be a cache hit
    add r1 3 r3 ; r3=10
    write 7 r2 r3 ; Write 10 to 2nd cache line with offset 7 (last word of cache line)

    ; Now read both back
    read 8 r0 r5 ; Read SDRAM address 8 into r5, should be 7

    read 15 r0 r6 ; Read SDRAM address 15 into r6, should be 10
    
    add r5 r6 r15 ; expected=17

    halt
