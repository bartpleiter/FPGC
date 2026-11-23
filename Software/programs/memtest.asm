; Memory test, a program that the boot ROM loads over the UART.
; Tests the SDRAM behind this program, from 0x10000 to the end at 64 MiB, through the
; caches, in three passes: each word gets its address, then its address inverted, then
; a pseudo-random word (xorshift32). Each pass writes every word, writes the caches back
; with ccache, so the reads come from the SDRAM, and reads every word back. Prints the
; first 16 errors, a line per pass, and the number of errors, and returns to the boot
; ROM. The passes take a few seconds each.
;
; Registers: r6 the end, r7 the address, r8 the expected word, r9 the random state,
; r11 the errors, r12 the pass, r13 the start. Pattern is called with r14, and the
; print functions with r15; they use r1 to r4.

    load32 0x00010000 r13       ; START
    load32 0x04000000 r6        ; END
    load 0 r11
    load 0 r12

Pass:
    or r13 r0 r7
    load32 0x2545F491 r9        ; the random seed
Write:
    savpc 8 r14
    jump Pattern
    write 0 r7 r8
    add r7 4 r7
    bne r7 r6 Write
    ccache                      ; to the SDRAM, and out of the caches

    or r13 r0 r7
    load32 0x2545F491 r9
Check:
    savpc 8 r14
    jump Pattern
    read 0 r7 r10
    beq r8 r10 CheckNext
    add r11 1 r11
    load 16 r2
    bgt r11 r2 CheckNext        ; only the first 16 errors are printed
    addr2reg TextError r1
    savpc 8 r15
    jump PrintText
    or r7 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextWrote r1
    savpc 8 r15
    jump PrintText
    or r8 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextRead r1
    savpc 8 r15
    jump PrintText
    or r10 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
CheckNext:
    add r7 4 r7
    bne r7 r6 Check

    addr2reg TextPass r1
    savpc 8 r15
    jump PrintText
    or r12 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextErrors r1
    savpc 8 r15
    jump PrintText
    or r11 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
    add r12 1 r12
    load 3 r2
    bne r12 r2 Pass

    addr2reg TextDone r1
    savpc 8 r15
    jump PrintText
    or r11 r0 r1
    savpc 8 r15
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r15
    jump PrintText
    jump 0x04000000             ; back to the boot ROM

; The word for address r7 in pass r12 into r8. Pass 2 takes the next random word from
; r9. Uses r2. Returns with r14.
Pattern:
    beq r12 r0 PatternAddress
    load 1 r2
    beq r12 r2 PatternInverted
    shiftl r9 13 r2             ; xorshift32
    xor r9 r2 r9
    shiftr r9 17 r2
    xor r9 r2 r9
    shiftl r9 5 r2
    xor r9 r2 r9
    or r9 r0 r8
    jumpr 0 r14
PatternAddress:
    or r7 r0 r8
    jumpr 0 r14
PatternInverted:
    not r7 r8
    jumpr 0 r14

; Prints the text at r1, one character per word, up to a 0. Uses r1 and r2.
PrintText:
    read 0 r1 r2
    beq r2 r0 PrintTextDone
    write 0xFFFF8000 r0 r2      ; UART TRANSMIT
    add r1 4 r1
    jump PrintText
PrintTextDone:
    jumpr 0 r15

; Prints r1 as 8 hex digits. Uses r1 to r4.
PrintHex:
    load 8 r3                   ; digits left
    load 10 r4
PrintHexNext:
    shiftr r1 28 r2             ; the highest digit
    shiftl r1 4 r1
    blt r2 r4 PrintHexDigit
    add r2 0x27 r2              ; a to f: 0x61 is 0x30 + 10 + 0x27
PrintHexDigit:
    add r2 0x30 r2
    write 0xFFFF8000 r0 r2      ; UART TRANSMIT
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r15

TextError:     ; "memtest: error at "
    .dw 0x6D 0x65 0x6D 0x74 0x65 0x73 0x74 0x3A 0x20 0x65 0x72 0x72 0x6F 0x72 0x20 0x61
    .dw 0x74 0x20 0
TextWrote:     ; " wrote "
    .dw 0x20 0x77 0x72 0x6F 0x74 0x65 0x20 0
TextRead:      ; " read "
    .dw 0x20 0x72 0x65 0x61 0x64 0x20 0
TextPass:      ; "memtest: pass "
    .dw 0x6D 0x65 0x6D 0x74 0x65 0x73 0x74 0x3A 0x20 0x70 0x61 0x73 0x73 0x20 0
TextErrors:    ; " done, errors "
    .dw 0x20 0x64 0x6F 0x6E 0x65 0x2C 0x20 0x65 0x72 0x72 0x6F 0x72 0x73 0x20 0
TextDone:      ; "memtest: done, errors "
    .dw 0x6D 0x65 0x6D 0x74 0x65 0x73 0x74 0x3A 0x20 0x64 0x6F 0x6E 0x65 0x2C 0x20 0x65
    .dw 0x72 0x72 0x6F 0x72 0x73 0x20 0
TextNewline:   ; "\r\n"
    .dw 0x0D 0x0A 0
