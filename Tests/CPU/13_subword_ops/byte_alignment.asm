; Test byte addressing within a single word
; Write a word, then read individual bytes at offsets 0, 1, 2, 3
Main:
    ; Write 0x44332211 to byte address 0
    load32 0x44332211 r1
    write 0 r0 r1

    ; Byte 0 should be 0x11
    readbu 0 r0 r2
    load 0x11 r3
    beq r2 r3 Pass1
    load 1 r15  ; FAIL
    halt

Pass1:
    ; Byte 1 should be 0x22
    readbu 1 r0 r4
    load 0x22 r5
    beq r4 r5 Pass2
    load 2 r15  ; FAIL
    halt

Pass2:
    ; Byte 2 should be 0x33
    readbu 2 r0 r6
    load 0x33 r7
    beq r6 r7 Pass3
    load 3 r15  ; FAIL
    halt

Pass3:
    ; Byte 3 should be 0x44
    readbu 3 r0 r8
    load 0x44 r9
    beq r8 r9 Pass4
    load 4 r15  ; FAIL
    halt

Pass4:
    load 42 r15  ; expected=42
    halt
