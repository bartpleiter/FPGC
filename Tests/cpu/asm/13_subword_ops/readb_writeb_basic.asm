; Test basic byte write and byte read (signed and unsigned)
; Write 0xAB to byte address 0, read it back with readb and readbu
Main:
    ; Write word 0xDEAD00AB to address 0 (so byte 0 = 0xAB)
    load32 0xDEAD00AB r1
    write 0 r0 r1

    ; Read byte 0 with readb (sign-extend): 0xAB = -85 signed → 0xFFFFFFAB
    readb 0 r0 r2
    load32 0xFFFFFFAB r3
    beq r2 r3 Pass1
    load 1 r15  ; FAIL
    halt

Pass1:
    ; Read byte 0 with readbu (zero-extend): 0xAB → 0x000000AB
    readbu 0 r0 r4
    load32 0x000000AB r5
    beq r4 r5 Pass2
    load 2 r15  ; FAIL
    halt

Pass2:
    ; Write byte 0x42 to byte address 1 using writeb
    load 0x42 r6
    writeb 1 r0 r6

    ; Read it back with readbu
    readbu 1 r0 r7
    beq r7 r6 Pass3
    load 3 r15  ; FAIL
    halt

Pass3:
    ; Verify that byte 0 wasn't corrupted by writeb to byte 1
    readbu 0 r0 r8
    beq r8 r4 Pass4
    load 4 r15  ; FAIL
    halt

Pass4:
    load 42 r15  ; expected=42
    halt
