Main:
    ; Note: if you want to use jumps, then only use offsets, unless you configure the assembler with the correct ROM base address
    nop
    nop
    nop
    nop
    load 7 r1
    load 8 r2
    add r1 r2 r3 ; r3=15
    read 3 r0 r12 ; read RAM address 3 into r12
    ;write 0 r0 r3 ; write 15 to RAM address 0
    nop
    nop
    nop
    nop
    add r12 1 r12;
    nop
    nop
    read 5 r0 r5 ; read RAM address 5 into r5
    nop
    nop
    nop
    nop
    nop
    add r5 1 r5
    nop
    nop
    nop
    nop
    write 30 r0 r3 ; write 15 to RAM address 64
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    read 30 r0 r4 ; read RAM address 30 into r4
    nop
    nop
    nop
    nop



    halt
