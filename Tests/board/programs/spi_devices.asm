; A first command on every SPI device of the board, loaded by the boot ROM. Prints a
; line per device and returns to the boot ROM:
;
;   flash1 001840ef   the JEDEC ID (EFh, 40h, 18h)
;   flash2 001840ef
;   usb1 000000aa     CHECK_EXIST with 55h gives its inverse
;   usb2 000000aa
;   eth 00000034      ERDPTL written with 34h and read back
;   sd 00000001       the answer to CMD0: idle
;
; Registers: r10 the bus. The device functions are called with r14, Print and Delay
; with r15, and PrintText and PrintHex with r13.

.equ UART_TRANSMIT 0xFFFF8000
.equ FLASH1 0xFFFF8400
.equ FLASH2 0xFFFF8500
.equ USB1 0xFFFF8600
.equ USB2 0xFFFF8700
.equ ETH 0xFFFF8800
.equ SD_CARD 0xFFFF8900
.equ BOOT_ROM 0x04000000
.equ BYTE 0
.equ WORD 4
.equ CONTROL 8

    load32 FLASH1 r10
    savpc 8 r14
    jump FlashId
    addr2reg TextFlash1 r1
    savpc 8 r15
    jump Print

    load32 FLASH2 r10
    savpc 8 r14
    jump FlashId
    addr2reg TextFlash2 r1
    savpc 8 r15
    jump Print

    load32 USB1 r10
    savpc 8 r14
    jump CheckExist
    addr2reg TextUsb1 r1
    savpc 8 r15
    jump Print

    load32 USB2 r10
    savpc 8 r14
    jump CheckExist
    addr2reg TextUsb2 r1
    savpc 8 r15
    jump Print

    ; ENC28J60: bank 0 (clear BSEL in ECON1), write ERDPTL, read it back
    load32 ETH r10
    load 1 r1
    write CONTROL r10 r1
    load 0xBF r1                ; Bit Field Clear of ECON1 (1Fh)
    write BYTE r10 r1
    load 0x03 r1                ; BSEL1 and BSEL0
    write BYTE r10 r1
    write CONTROL r10 r0
    savpc 8 r15                 ; the chip select high for at least 50 ns
    jump Delay
    load 1 r1
    write CONTROL r10 r1
    load 0x40 r1                ; Write Control Register, ERDPTL (00h)
    write BYTE r10 r1
    load 0x34 r1
    write BYTE r10 r1
    write CONTROL r10 r0
    savpc 8 r15
    jump Delay
    load 1 r1
    write CONTROL r10 r1
    load 0x00 r1                ; Read Control Register, ERDPTL
    write BYTE r10 r1
    read BYTE r10 r5
    write CONTROL r10 r0
    addr2reg TextEth r1
    savpc 8 r15
    jump Print

    ; SD card: 80 clocks with the chip select high, then CMD0, and the first byte of
    ; its answer that isn't FFh
    load32 SD_CARD r10
    load 10 r6
SdClocks:
    read BYTE r10 r1
    sub r6 1 r6
    bne r6 r0 SdClocks
    load 1 r1
    write CONTROL r10 r1
    load32 0x00000040 r1        ; CMD0, and 3 bytes of the argument 0
    write WORD r10 r1
    write BYTE r10 r0           ; the argument's last byte
    load 0x95 r1                ; the CRC of CMD0
    write BYTE r10 r1
    load 0xFF r2
    load 10 r6
SdAnswer:
    read BYTE r10 r5
    bne r5 r2 SdDone
    sub r6 1 r6
    bne r6 r0 SdAnswer
SdDone:
    write CONTROL r10 r0
    read BYTE r10 r1            ; 8 more clocks after the chip select
    addr2reg TextSd r1
    savpc 8 r15
    jump Print

    jump BOOT_ROM

; The JEDEC ID of the flash on bus r10 into r5. Uses r1, r2. Returns with r14.
FlashId:
    load 1 r1
    write CONTROL r10 r1
    load 0x9F r1
    write BYTE r10 r1
    read BYTE r10 r5
    read BYTE r10 r2
    shiftl r2 8 r2
    or r5 r2 r5
    read BYTE r10 r2
    shiftl r2 16 r2
    or r5 r2 r5
    write CONTROL r10 r0
    jumpr 0 r14

; CHECK_EXIST (06h) with 55h on the CH376 on bus r10, its answer into r5. The CH376
; needs 1.5 us after each byte. Uses r1, r9. Returns with r14.
CheckExist:
    load 1 r1
    write CONTROL r10 r1
    load 0x06 r1
    write BYTE r10 r1
    savpc 8 r15
    jump Delay
    load 0x55 r1
    write BYTE r10 r1
    savpc 8 r15
    jump Delay
    read BYTE r10 r5
    write CONTROL r10 r0
    jumpr 0 r14

; About 2 us. Uses r9. Returns with r15.
Delay:
    load 50 r9
DelayNext:
    sub r9 1 r9
    bne r9 r0 DelayNext
    jumpr 0 r15

; Prints the text at r1, r5 in hex, and a new line. Returns with r15.
Print:
    savpc 8 r13
    jump PrintText
    or r5 r0 r1
    savpc 8 r13
    jump PrintHex
    addr2reg TextNewline r1
    savpc 8 r13
    jump PrintText
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

TextFlash1:
    .ds "flash1 "
TextFlash2:
    .ds "flash2 "
TextUsb1:
    .ds "usb1 "
TextUsb2:
    .ds "usb2 "
TextEth:
    .ds "eth "
TextSd:
    .ds "sd "
TextNewline:
    .ds "\r\n"
