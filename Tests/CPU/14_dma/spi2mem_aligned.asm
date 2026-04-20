; Test: DMA SPI2MEM aligned 32-byte read from W25Q128JV simulation model
;
; Verifies the DMAengine SPI2MEM mode (step 9) can read 32 bytes from SPI0
; (Flash 1) into a 32-byte-aligned SDRAM destination.
;
; Sequence:
;   1) Drive CS=0, send READ_DATA (0x03) + 24-bit address (0,0,0) byte by byte
;      via the existing single-byte SPI MMIO interface.
;   2) Program DMA: SRC=0 (ignored), DST=0x2000, COUNT=32,
;      CTRL=start | mode=SPI2MEM(2) | spi_id=0.
;   3) Poll DMA_STATUS busy bit until it clears.
;   4) Drive CS=1.
;   5) ccached, then OR-reduce 8 destination words into r15.
;
; Expected = OR of the first 32 bytes of spiflash1.list interpreted as 8
; little-endian 32-bit words. With the current spiflash1.list contents this
; works out to 0xFFCB3E9F = 4291509919.
;
; expected=4291509919

Main:
    load32 0x2000 r2          ; dst byte addr (line-aligned)
    load 0 r4                 ; sentinel for dst init

    ; --- 1) Initialise dst region with sentinel (so a no-op DMA would fail) ---
    write 0  r2 r4
    write 4  r2 r4
    write 8  r2 r4
    write 12 r2 r4
    write 16 r2 r4
    write 20 r2 r4
    write 24 r2 r4
    write 28 r2 r4

    ; Make sure the sentinel is in SDRAM, not just sitting in L1d.
    ccached

    ; --- 2) Drive CS low and send command + address bytes via SPI0 ---
    load32 0x1C000020 r5      ; ADDR_SPI0_DATA
    load32 0x1C000024 r6      ; ADDR_SPI0_CS

    write 0 r6 r0             ; CS = 0 (select)

    load 3 r7                 ; READ_DATA (0x03)
    write 0 r5 r7             ; spi tx 0x03

    write 0 r5 r0             ; addr[23:16] = 0x00
    write 0 r5 r0             ; addr[15:8]  = 0x00
    write 0 r5 r0             ; addr[7:0]   = 0x00

    ; --- 3) Program the DMA engine via MMIO ---
    load32 0x1C000070 r8      ; DMA register block base (SRC)
    write 0  r8 r0            ; DMA_SRC   = 0 (ignored for SPI2MEM)
    write 4  r8 r2            ; DMA_DST   = 0x2000
    load 32 r9
    write 8  r8 r9            ; DMA_COUNT = 32
    ; CTRL: bit 31=start, bits[7:5]=spi_id (0=SPI0), bits[3:0]=mode (2=SPI2MEM)
    load32 0x80000002 r10
    write 12 r8 r10           ; DMA_CTRL  = start | SPI2MEM | spi_id=0

    ; --- 4) Poll DMA_STATUS until busy bit (bit 0) clears ---
    load 1 r11                ; busy mask
PollBusy:
    read 16 r8 r12            ; r12 = STATUS (offset 0x10 from SRC base)
    and r12 r11 r13
    bne r13 r0 PollBusy

    ; --- 5) CS = 1 (deselect) ---
    load 1 r14
    write 0 r6 r14

    ; --- 6) Invalidate L1d so reads from dst come from SDRAM ---
    ccached

    ; --- 7) OR-reduce all 8 dst words into r15 ---
    read 0  r2 r10
    read 4  r2 r11
    read 8  r2 r12
    read 12 r2 r13
    or r10 r11 r14
    or r14 r12 r14
    or r14 r13 r14
    read 16 r2 r10
    read 20 r2 r11
    read 24 r2 r12
    read 28 r2 r13
    or r14 r10 r14
    or r14 r11 r14
    or r14 r12 r14
    or r14 r13 r15            ; expected: 0xFFCB3F9F

    halt
