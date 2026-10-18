; This test is for a specific case where some cache miss reads were passed to WB stage as cache hits
Main:

    load 0 r15
    load 0 r1

    ; Write pattern to SDRAM (byte-addressed: each word is 4 bytes apart)

    write 0 r0 r1
    add r1 1 r1
    write 4 r0 r1
    add r1 1 r1
    write 8 r0 r1
    add r1 1 r1
    write 12 r0 r1
    add r1 1 r1
    write 16 r0 r1
    add r1 1 r1
    write 20 r0 r1
    add r1 1 r1
    write 24 r0 r1
    add r1 1 r1
    write 28 r0 r1
    add r1 1 r1
    write 32 r0 r1
    add r1 1 r1
    write 36 r0 r1
    add r1 1 r1
    write 40 r0 r1
    add r1 1 r1
    write 44 r0 r1
    add r1 1 r1
    write 48 r0 r1
    add r1 1 r1
    write 52 r0 r1
    add r1 1 r1
    write 56 r0 r1
    add r1 1 r1
    write 60 r0 r1

    ; Evict cache to force write-back of dirty lines
    ; Addresses 4096-4156 alias to same cache indices as 0-60

    nop
    write 4096 r0 r0
    write 4100 r0 r0
    write 4104 r0 r0
    write 4108 r0 r0
    write 4112 r0 r0
    write 4116 r0 r0
    write 4120 r0 r0
    write 4124 r0 r0
    write 4128 r0 r0
    write 4132 r0 r0
    write 4136 r0 r0
    write 4140 r0 r0
    write 4144 r0 r0
    write 4148 r0 r0
    write 4152 r0 r0
    write 4156 r0 r0

    ; Read from SDRAM and write to VRAM

    
    load32 0x1EC00000 r1 ; VRAMpx base address and counter
    
    read 0 r0 r2
    write 0 r1 r2
    read 4 r0 r2
    write 4 r1 r2
    read 8 r0 r2
    write 8 r1 r2
    read 12 r0 r2
    write 12 r1 r2
    read 16 r0 r2
    write 16 r1 r2
    read 20 r0 r2
    write 20 r1 r2
    read 24 r0 r2
    write 24 r1 r2
    read 28 r0 r2
    write 28 r1 r2
    read 32 r0 r2
    write 32 r1 r2
    
    or r2 r15 r15 ; expected=8
    
    halt
