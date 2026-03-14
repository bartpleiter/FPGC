; Tests SDRAM memory read/write operations with l1d cache
; Assumes cache line size of 8 words (32 bytes), meaning that byte address 32 is on the 2nd cache line
Main:
    ; Semi random read/writes with the goal to create difficult situations

    load 7 r1 ; r1 = 7
    load 8 r2 ; r2 = 8

    write 32 r0 r1 ; mem[32] = 7 (2nd cache line)
    write 36 r0 r2 ; mem[36] = 8
    read 32 r0 r3 ; r3 = mem[32] = 7
    add r1 r3 r2 ; r2 = 7+7=14
    write 68 r0 r2 ; mem[68] = 14 (3rd cache line)
    read 36 r0 r4 ; r4 = mem[36] = 8
    multu r2 r4 r5 ; r5 = 14*8=112
    write 28 r0 r5 ; mem[28] = 112 (1st cache line)
    
    read 28 r0 r6 ; r6 = mem[28] = 112
    read 68 r0 r7 ; r7 = mem[68] = 14
    add r6 r7 r15 ; expected=126

    halt
