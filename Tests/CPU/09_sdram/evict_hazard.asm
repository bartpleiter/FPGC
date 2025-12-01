; This test is for a specific case where some cache miss reads were passed to WB stage as cache hits
Main:

    load 0 r15
    load 0 r1

    ; Write pattern to SDRAM

    write 0 r0 r1
    add r1 1 r1
    write 1 r0 r1
    add r1 1 r1
    write 2 r0 r1
    add r1 1 r1
    write 3 r0 r1
    add r1 1 r1
    write 4 r0 r1
    add r1 1 r1
    write 5 r0 r1
    add r1 1 r1
    write 6 r0 r1
    add r1 1 r1
    write 7 r0 r1
    add r1 1 r1
    write 8 r0 r1
    add r1 1 r1
    write 9 r0 r1
    add r1 1 r1
    write 10 r0 r1
    add r1 1 r1
    write 11 r0 r1
    add r1 1 r1
    write 12 r0 r1
    add r1 1 r1
    write 13 r0 r1
    add r1 1 r1
    write 14 r0 r1
    add r1 1 r1
    write 15 r0 r1

    ; Evict cache to force write-back of dirty lines

    nop
    write 1024 r0 r0
    write 1025 r0 r0
    write 1026 r0 r0
    write 1027 r0 r0
    write 1028 r0 r0
    write 1029 r0 r0
    write 1030 r0 r0
    write 1031 r0 r0
    write 1032 r0 r0
    write 1033 r0 r0
    write 1034 r0 r0
    write 1035 r0 r0
    write 1036 r0 r0
    write 1037 r0 r0
    write 1038 r0 r0
    write 1039 r0 r0

    ; Read from SDRAM and write to VRAM

    
    load32 0x7B00000 r1 ; VRAMpx base address and counter
    
    read 0 r0 r2
    write 0 r1 r2
    read 1 r0 r2
    write 1 r1 r2
    read 2 r0 r2
    write 2 r1 r2
    read 3 r0 r2
    write 3 r1 r2
    read 4 r0 r2
    write 4 r1 r2
    read 5 r0 r2
    write 5 r1 r2
    read 6 r0 r2
    write 6 r1 r2
    read 7 r0 r2
    write 7 r1 r2
    read 8 r0 r2
    write 8 r1 r2
    
    or r2 r15 r15 ; expected=8
    
    halt
        
    
