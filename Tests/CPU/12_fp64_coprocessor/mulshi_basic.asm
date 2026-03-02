Main:
    ; Test mulshi: signed 32x32 multiply, return high 32 bits
    ; 0x10000 * 0x30000 = 0x300000000 (full 64-bit result)
    ; High 32 bits = 3, Low 32 bits = 0
    load32 0x10000 r1
    load32 0x30000 r2
    mulshi r1 r2 r3       ; r3 should be 3

    add r3 r0 r15         ; expected=3
    
    halt
