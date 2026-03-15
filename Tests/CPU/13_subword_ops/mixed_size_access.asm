; Test mixed-size access: write word, read bytes; write bytes, read word
Main:
    ; Write a full word
    load32 0xAABBCCDD r1
    write 0 r0 r1

    ; Read it back as individual bytes and sum them
    readbu 0 r0 r2   ; byte 0 = 0xDD = 221
    readbu 1 r0 r3   ; byte 1 = 0xCC = 204
    readbu 2 r0 r4   ; byte 2 = 0xBB = 187
    readbu 3 r0 r5   ; byte 3 = 0xAA = 170

    ; Now write individual bytes to address 4
    load 0x11 r6
    writeb 4 r0 r6   ; byte 0 = 0x11
    load 0x22 r7
    writeb 5 r0 r7   ; byte 1 = 0x22
    load 0x33 r8
    writeb 6 r0 r8   ; byte 2 = 0x33
    load 0x44 r9
    writeb 7 r0 r9   ; byte 3 = 0x44

    ; Read back as a full word — should be 0x44332211
    read 4 r0 r10
    load32 0x44332211 r11
    beq r10 r11 Pass1
    load 1 r15  ; FAIL
    halt

Pass1:
    ; Sum the original bytes: 221 + 204 + 187 + 170 = 782
    add r2 r3 r12
    add r12 r4 r12
    add r12 r5 r15  ; expected=782
    halt
