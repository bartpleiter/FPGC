; The SPI buses with the testbench's flash models (Tests/io/devices.vh): flash 1's JEDEC
; ID, then a sector erase, a page program of four words, and reads of them with 03h,
; 0Bh and 6Bh, each word checked. Then a quad read of flash 2, which is erased, and a
; word from the SD card's bus, which has no device and reads 1s. Prints the ID and the
; number of errors.
;
; Registers: r10 the bus, r11 the errors, r5 the ID. Check is called with r15, the
; flash functions with r14, and WaitDone with r13.
; devices
; uart output "spi: id 001840ef errors 00000000\r\n"

.equ UART_TRANSMIT 0xFFFF8000
.equ FLASH1 0xFFFF8400
.equ FLASH2 0xFFFF8500
.equ SD_CARD 0xFFFF8900
.equ BYTE 0
.equ WORD 4
.equ CONTROL 8

    load 0 r11
    load32 FLASH1 r10

    ; The JEDEC ID, three bytes
    savpc 8 r14
    jump Select
    load 0x9F r1
    write BYTE r10 r1
    read BYTE r10 r5
    read BYTE r10 r2
    shiftl r2 8 r2
    or r5 r2 r5
    read BYTE r10 r2
    shiftl r2 16 r2
    or r5 r2 r5
    savpc 8 r14
    jump Deselect

    ; Erase the sector at 0x1000
    savpc 8 r14
    jump WriteEnable
    savpc 8 r14
    jump Select
    load32 0x00100020 r1        ; 20h, address 0x001000
    write WORD r10 r1
    savpc 8 r14
    jump Deselect
    savpc 8 r13
    jump WaitDone

    ; Program the four words of Pattern at 0x1004
    savpc 8 r14
    jump WriteEnable
    savpc 8 r14
    jump Select
    load32 0x04100002 r1        ; 02h, address 0x001004
    write WORD r10 r1
    addr2reg Pattern r6
    load 4 r7
Program:
    read 0 r6 r1
    write WORD r10 r1
    add r6 4 r6
    sub r7 1 r7
    bne r7 r0 Program
    savpc 8 r14
    jump Deselect
    savpc 8 r13
    jump WaitDone

    ; 03h from 0x1000: an erased word, the four words, an erased word
    savpc 8 r14
    jump Select
    load32 0x00100003 r1
    write WORD r10 r1
    not r0 r3                   ; all 1s
    read WORD r10 r2
    savpc 8 r15
    jump Check
    addr2reg Pattern r6
    load 4 r7
ReadSlow:
    read WORD r10 r2
    read 0 r6 r3
    savpc 8 r15
    jump Check
    add r6 4 r6
    sub r7 1 r7
    bne r7 r0 ReadSlow
    not r0 r3                   ; all 1s
    read WORD r10 r2
    savpc 8 r15
    jump Check
    savpc 8 r14
    jump Deselect

    ; 0Bh from 0x1004: a dummy byte, then the first word
    savpc 8 r14
    jump Select
    load32 0x0410000B r1
    write WORD r10 r1
    read BYTE r10 r2
    read WORD r10 r2
    addr2reg Pattern r6
    read 0 r6 r3
    savpc 8 r15
    jump Check
    savpc 8 r14
    jump Deselect

    ; 6Bh from 0x1004: the dummy clocks as a quad word, then the four words
    savpc 8 r14
    jump Select
    load32 0x0410006B r1
    write WORD r10 r1
    load 3 r1                   ; SELECT and QUAD
    write CONTROL r10 r1
    read WORD r10 r2
    addr2reg Pattern r6
    load 4 r7
ReadQuad:
    read WORD r10 r2
    read 0 r6 r3
    savpc 8 r15
    jump Check
    add r6 4 r6
    sub r7 1 r7
    bne r7 r0 ReadQuad
    savpc 8 r14
    jump Deselect

    ; 6Bh from flash 2's address 0, which is erased
    load32 FLASH2 r10
    savpc 8 r14
    jump Select
    load 0x6B r1
    write WORD r10 r1
    load 3 r1
    write CONTROL r10 r1
    read WORD r10 r2
    read WORD r10 r2
    not r0 r3                   ; all 1s
    savpc 8 r15
    jump Check
    savpc 8 r14
    jump Deselect

    ; A word from the SD card's bus, without a device
    load32 SD_CARD r10
    read WORD r10 r2
    not r0 r3                   ; all 1s
    savpc 8 r15
    jump Check

    addr2reg TextId r1
    savpc 8 r15
    jump PrintText
    or r5 r0 r1
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
    halt

; Counts an error when r2 isn't r3. Returns with r15.
Check:
    beq r2 r3 CheckDone
    add r11 1 r11
CheckDone:
    jumpr 0 r15

; The chip select of bus r10 low, or high. Use r1. Return with r14.
Select:
    load 1 r1
    write CONTROL r10 r1
    jumpr 0 r14
Deselect:
    write CONTROL r10 r0
    jumpr 0 r14

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

; Prints the string at r1, up to its 0. Uses r1 and r2. Returns with r15.
PrintText:
    readbu 0 r1 r2
    beq r2 r0 PrintTextDone
    write UART_TRANSMIT r0 r2
    add r1 1 r1
    jump PrintText
PrintTextDone:
    jumpr 0 r15

; Prints r1 as 8 hex digits. Uses r1 to r4. Returns with r15.
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
    jumpr 0 r15

Pattern:
    .dw 0x12345678 0x9ABCDEF0 0x0F1E2D3C 0xA5C3E187
TextId:
    .ds "spi: id "
TextErrors:
    .ds " errors "
TextNewline:
    .ds "\r\n"
