; Test halfword write and halfword read (signed and unsigned)
; Write halfwords and verify sign/zero extension
Main:
    ; Write word 0x8000BEEF to address 0
    load32 0x8000BEEF r1
    write 0 r0 r1

    ; Read low halfword (offset 0) with readh (sign-extend): 0xBEEF → 0xFFFFBEEF
    readh 0 r0 r2
    load32 0xFFFFBEEF r3
    beq r2 r3 Pass1
    load 1 r15  ; FAIL
    halt

Pass1:
    ; Read low halfword with readhu (zero-extend): 0xBEEF → 0x0000BEEF
    readhu 0 r0 r4
    load32 0x0000BEEF r5
    beq r4 r5 Pass2
    load 2 r15  ; FAIL
    halt

Pass2:
    ; Read high halfword (offset 2) with readh: 0x8000 → 0xFFFF8000 (sign-extend)
    readh 2 r0 r6
    load32 0xFFFF8000 r7
    beq r6 r7 Pass3
    load 3 r15  ; FAIL
    halt

Pass3:
    ; Read high halfword with readhu: 0x8000 → 0x00008000 (zero-extend)
    readhu 2 r0 r8
    load32 0x00008000 r9
    beq r8 r9 Pass4
    load 4 r15  ; FAIL
    halt

Pass4:
    ; Write halfword 0x1234 to byte offset 2 using writeh
    load32 0x1234 r10
    writeh 2 r0 r10

    ; Read it back
    readhu 2 r0 r11
    beq r11 r10 Pass5
    load 5 r15  ; FAIL
    halt

Pass5:
    ; Verify low halfword wasn't corrupted
    readhu 0 r0 r12
    beq r12 r4 Pass6
    load 6 r15  ; FAIL
    halt

Pass6:
    load 42 r15  ; expected=42
    halt
