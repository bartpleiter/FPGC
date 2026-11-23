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

.equ UART_TRANSMIT 0xFFFF8000
.equ BOOT_ROM 0x04000000
.equ START 0x00010000
.equ END 0x04000000
.equ SEED 0x2545F491

    load32 START r13
    load32 END r6
    load 0 r11
    load 0 r12

Pass:
    or r13 r0 r7
    load32 SEED r9
Write:
    savpc 8 r14
    jump Pattern
    write 0 r7 r8
    add r7 4 r7
    bne r7 r6 Write
    ccache                      ; to the SDRAM, and out of the caches

    or r13 r0 r7
    load32 SEED r9
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
    jump BOOT_ROM

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

; Prints the string at r1, up to its 0. Uses r1 and r2.
PrintText:
    readbu 0 r1 r2
    beq r2 r0 PrintTextDone
    write UART_TRANSMIT r0 r2
    add r1 1 r1
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
    add r2 39 r2                ; a to f: 'a' is '0' + 10 + 39
PrintHexDigit:
    add r2 '0' r2
    write UART_TRANSMIT r0 r2
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r15

TextError:
    .ds "memtest: error at "
TextWrote:
    .ds " wrote "
TextRead:
    .ds " read "
TextPass:
    .ds "memtest: pass "
TextErrors:
    .ds " done, errors "
TextDone:
    .ds "memtest: done, errors "
TextNewline:
    .ds "\r\n"
