; All 16 MiB of flash 2, loaded by the boot ROM: erases its 256 blocks of 64 KiB,
; programs every page with a pseudo-random pattern (xorshift32), then copies the whole
; flash to SDRAM twice: with Fast Read Quad Output (6Bh) to 0x01000000, and with Read
; Data (03h) to 0x02000000. Each copy is timed with the cycle counter, and checked
; against the pattern. Prints:
;
;   flash2 erased
;   flash2 programmed
;   flash2 quad errors 00000000 cycles <n>
;   flash2 single errors 00000000 cycles <n>
;
; The erase and the program take about a minute and a half, set by the chip.
;
; Registers: r10 the bus, r9 the random state, r7 and r6 an address and its end, r11
; and r12 the quad copy's cycles and errors, r8 the single copy's cycles. The
; flash functions are called with r14 (WaitDone with r13), Command and Print with r15,
; and PrintText and PrintHex with r13.

.equ UART_TRANSMIT 0xFFFF8000
.equ COUNTER_SNAPSHOT 0xFFFF8100
.equ COUNTER_LOW 0xFFFF8104
.equ FLASH2 0xFFFF8500
.equ BOOT_ROM 0x04000000
.equ BYTE 0
.equ WORD 4
.equ CONTROL 8
.equ SEED 0x2545F491
.equ SIZE 0x01000000
.equ QUAD_COPY 0x01000000
.equ QUAD_END 0x02000000
.equ SINGLE_COPY 0x02000000
.equ SINGLE_END 0x03000000

    load32 FLASH2 r10

    ; Erase every block
    load 0 r7
    load32 SIZE r6
EraseBlock:
    savpc 8 r14
    jump WriteEnable
    load 1 r1
    write CONTROL r10 r1
    load 0xD8 r1
    or r7 r0 r2
    savpc 8 r15
    jump Command
    write WORD r10 r1
    write CONTROL r10 r0
    savpc 8 r13
    jump WaitDone
    load32 0x10000 r1
    add r7 r1 r7
    bne r7 r6 EraseBlock
    addr2reg TextErased r1
    savpc 8 r13
    jump PrintText

    ; Program every page
    load 0 r7
    load32 SEED r9
ProgramPage:
    savpc 8 r14
    jump WriteEnable
    load 1 r1
    write CONTROL r10 r1
    load 0x02 r1
    or r7 r0 r2
    savpc 8 r15
    jump Command
    write WORD r10 r1
    load 64 r8
ProgramWord:
    shiftl r9 13 r2             ; xorshift32
    xor r9 r2 r9
    shiftr r9 17 r2
    xor r9 r2 r9
    shiftl r9 5 r2
    xor r9 r2 r9
    write WORD r10 r9
    sub r8 1 r8
    bne r8 r0 ProgramWord
    write CONTROL r10 r0
    savpc 8 r13
    jump WaitDone
    add r7 256 r7
    bne r7 r6 ProgramPage
    addr2reg TextProgrammed r1
    savpc 8 r13
    jump PrintText

    ; The quad copy: the command, QUAD on, the 8 dummy clocks as a word, the data
    write COUNTER_SNAPSHOT r0 r0
    read COUNTER_LOW r0 r11
    load 1 r1
    write CONTROL r10 r1
    load 0x6B r1
    write WORD r10 r1           ; 6Bh, address 0
    load 3 r1
    write CONTROL r10 r1
    read WORD r10 r1
    load32 QUAD_COPY r7
    load32 QUAD_END r6
    savpc 8 r14
    jump Copy
    write CONTROL r10 r0
    write COUNTER_SNAPSHOT r0 r0
    read COUNTER_LOW r0 r1
    sub r1 r11 r11
    load32 QUAD_COPY r7
    load32 QUAD_END r6
    savpc 8 r14
    jump Check
    or r4 r0 r12

    ; The single copy
    write COUNTER_SNAPSHOT r0 r0
    read COUNTER_LOW r0 r8
    load 1 r1
    write CONTROL r10 r1
    load 0x03 r1
    write WORD r10 r1           ; 03h, address 0
    load32 SINGLE_COPY r7
    load32 SINGLE_END r6
    savpc 8 r14
    jump Copy
    write CONTROL r10 r0
    write COUNTER_SNAPSHOT r0 r0
    read COUNTER_LOW r0 r1
    sub r1 r8 r8
    load32 SINGLE_COPY r7
    load32 SINGLE_END r6
    savpc 8 r14
    jump Check
    or r4 r0 r7                 ; PrintHex uses r4

    ; The results
    addr2reg TextQuad r1
    or r12 r0 r5
    savpc 8 r15
    jump Print
    addr2reg TextCycles r1
    or r11 r0 r5
    savpc 8 r15
    jump Print
    addr2reg TextNewline r1
    savpc 8 r13
    jump PrintText
    addr2reg TextSingle r1
    or r7 r0 r5
    savpc 8 r15
    jump Print
    addr2reg TextCycles r1
    or r8 r0 r5
    savpc 8 r15
    jump Print
    addr2reg TextNewline r1
    savpc 8 r13
    jump PrintText
    jump BOOT_ROM

; Copies words from bus r10 to r7, up to r6, 8 words at a time. Uses r1 to r3, r7.
; Returns with r14.
Copy:
    read WORD r10 r1
    write 0 r7 r1
    read WORD r10 r2
    write 4 r7 r2
    read WORD r10 r3
    write 8 r7 r3
    read WORD r10 r1
    write 12 r7 r1
    read WORD r10 r2
    write 16 r7 r2
    read WORD r10 r3
    write 20 r7 r3
    read WORD r10 r1
    write 24 r7 r1
    read WORD r10 r2
    write 28 r7 r2
    add r7 32 r7
    bne r7 r6 Copy
    jumpr 0 r14

; Counts the words from r7 up to r6 that aren't the pattern, into r4. Uses r1, r2, r9,
; r7. Returns with r14.
Check:
    load32 SEED r9
    load 0 r4
CheckNext:
    shiftl r9 13 r2
    xor r9 r2 r9
    shiftr r9 17 r2
    xor r9 r2 r9
    shiftl r9 5 r2
    xor r9 r2 r9
    read 0 r7 r1
    beq r1 r9 CheckOk
    add r4 1 r4
CheckOk:
    add r7 4 r7
    bne r7 r6 CheckNext
    jumpr 0 r14

; The command r1 with the address r2 as a word, the command first and the address's
; high byte next. Uses r3. Returns with r15.
Command:
    shiftr r2 16 r3
    and r3 0xFF r3
    shiftl r3 8 r3
    or r1 r3 r1
    shiftr r2 8 r3
    and r3 0xFF r3
    shiftl r3 16 r3
    or r1 r3 r1
    and r2 0xFF r3
    shiftl r3 24 r3
    or r1 r3 r1
    jumpr 0 r15

; Write Enable on bus r10. Uses r1. Returns with r14.
WriteEnable:
    load 1 r1
    write CONTROL r10 r1
    load 0x06 r1
    write BYTE r10 r1
    write CONTROL r10 r0
    jumpr 0 r14

; Polls the status of the flash on bus r10 until it isn't busy. Uses r1 and r2.
; Returns with r13.
WaitDone:
    load 1 r1
    write CONTROL r10 r1
    load 0x05 r1
    write BYTE r10 r1
WaitDoneNext:
    read BYTE r10 r2
    and r2 1 r2
    bne r2 r0 WaitDoneNext
    write CONTROL r10 r0
    jumpr 0 r13

; Prints the text at r1 and r5 in hex. Returns with r15.
Print:
    savpc 8 r13
    jump PrintText
    or r5 r0 r1
    savpc 8 r13
    jump PrintHex
    jumpr 0 r15

; Prints the string at r1, up to its 0. Uses r1 and r2. Returns with r13.
PrintText:
    readbu 0 r1 r2
    beq r2 r0 PrintTextDone
    write UART_TRANSMIT r0 r2
    add r1 1 r1
    jump PrintText
PrintTextDone:
    jumpr 0 r13

; Prints r1 as 8 hex digits. Uses r1 to r4. Returns with r13.
PrintHex:
    load 8 r3
    load 10 r4
PrintHexNext:
    shiftr r1 28 r2
    shiftl r1 4 r1
    blt r2 r4 PrintHexDigit
    add r2 39 r2
PrintHexDigit:
    add r2 '0' r2
    write UART_TRANSMIT r0 r2
    sub r3 1 r3
    bne r3 r0 PrintHexNext
    jumpr 0 r13

TextErased:
    .ds "flash2 erased\r\n"
TextProgrammed:
    .ds "flash2 programmed\r\n"
TextQuad:
    .ds "flash2 quad errors "
TextSingle:
    .ds "flash2 single errors "
TextCycles:
    .ds " cycles "
TextNewline:
    .ds "\r\n"
