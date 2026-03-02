Main:
    ; Test multuhi: unsigned 32x32 multiply, return high 32 bits
    ; 0x80000000 * 2 = 0x100000000
    ; High 32 bits = 1, Low 32 bits = 0
    load32 0x80000000 r1
    load 2 r2
    multuhi r1 r2 r3      ; r3 should be 1

    add r3 r0 r15         ; expected=1
    
    halt
