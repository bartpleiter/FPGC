Main:
    ; Test mulshi with negative numbers
    ; -1 * 0x10000 = -0x10000 = 0xFFFFFFFF_FFFF0000 (64-bit)
    ; High 32 bits = 0xFFFFFFFF = -1
    ; Low 32 bits  = 0xFFFF0000
    load32 -1 r1
    load32 0x10000 r2
    mulshi r1 r2 r3        ; r3 should be 0xFFFFFFFF = -1

    ; Verify by comparing: -1 + 2 = 1
    load 2 r4
    add r3 r4 r15          ; expected=1
    
    halt
