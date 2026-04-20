; Test: single-byte SPI MMIO read-back via SPI0 (Flash 1)
;
; Mirrors the bootloader / spi_flash.c pattern of:
;   write 0x20 byte   ; pulse a single-byte SPI transfer
;   read  0x20 byte   ; capture the received byte
;
; Sends READ_DATA (0x03) + 24-bit address (0,0,0) and reads the first 4
; bytes of spiflash1.list back into registers via single-byte transfers.
; Combines them as a little-endian 32-bit word.
;
; spiflash1.list bytes 0..3 are 0x90, 0x00, 0x00, 0x18
;   -> word = 0x18000090 = 402653328
;
; This regression test would have caught the SimpleSPI2 RX-FIFO auto-pop
; bug that broke the bootloader and BDOS unique-ID read on real hardware
; (see Docs/plans/dma-implementation-plan.md section 7 step 6).
;
; expected=402653328

Main:
    load32 0x1C000020 r5      ; ADDR_SPI0_DATA
    load32 0x1C000024 r6      ; ADDR_SPI0_CS

    write 0 r6 r0             ; CS = 0 (select)

    load 3 r7                 ; READ_DATA (0x03)
    write 0 r5 r7             ; spi tx 0x03
    write 0 r5 r0             ; addr[23:16] = 0x00
    write 0 r5 r0             ; addr[15:8]  = 0x00
    write 0 r5 r0             ; addr[7:0]   = 0x00

    ; Read byte 0 (LSB)
    write 0 r5 r0             ; dummy tx to clock in next byte
    read  0 r5 r1             ; r1 = byte 0

    ; Read byte 1
    write 0 r5 r0
    read  0 r5 r2             ; r2 = byte 1
    shiftl r2 8 r2

    ; Read byte 2
    write 0 r5 r0
    read  0 r5 r3             ; r3 = byte 2
    shiftl r3 16 r3

    ; Read byte 3 (MSB)
    write 0 r5 r0
    read  0 r5 r4             ; r4 = byte 3
    shiftl r4 24 r4

    ; Combine
    or r1 r2 r15
    or r15 r3 r15
    or r15 r4 r15

    ; CS high
    load 1 r7
    write 0 r6 r7

    halt
