; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache line size of 8 words (32 bytes), meaning that address 8 is on the 2nd cache line
Main:
    ; Semi random read/writes with the goal to create difficult situations

    load 7 r1 ; r1 = 7
    load 8 r2 ; r2 = 8

    write 0 r2 r1 ; mem[8] = 7
    write 1 r2 r2 ; mem[9] = 8
    read 0 r2 r3 ; r3 = mem[8] = 7
    add r1 r3 r2 ; r2 = 7+7=14
    write 3 r2 r2 ; mem[17] = 14
    read 9 r0 r4 ; r4 = mem[9] = 8
    multu r2 r4 r5 ; r5 = 14*8=112
    write 0 r1 r5 ; mem[7] = 112
    
    read 7 r0 r6 ; r6 = mem[7] = 112
    read 17 r0 r7 ; r7 = mem[17] = 14
    add r6 r7 r15 ; expected=126

    halt
