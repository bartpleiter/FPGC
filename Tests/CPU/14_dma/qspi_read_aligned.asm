; Test: DMA SPI2MEM_QSPI 32-byte Fast Read on SPI1
;
; Verifies the new MODE_SPI2MEM_QSPI (mode=6) wires the QSPI Fast Read
; controls (cmd_qspi_read + cmd_qspi_addr) through to QSPIflash and that
; the engine drains the resulting 32 RX bytes into SDRAM.
;
; Flash side: a small qspi_flash_sim slave is attached to SPI1's 4-bit
; bidirectional bus in the testbench. It returns
;     data[i] = (start_addr + i) & 0xFF
; in response to opcode 0xEB. We pick start_addr = 0x10 so the first
; byte is 0x10 and the 32 bytes ramp from 0x10 .. 0x2F.
;
; Sequence:
;   1) Sentinel-fill the 32-byte SDRAM destination so a no-op DMA fails.
;   2) Drive SPI1_CS low (the engine doesn't touch CS itself).
;   3) Program DMA: DST=0x2000, COUNT=32, QSPI_ADDR=0x10,
;      CTRL=start | mode=6 (SPI2MEM_QSPI) | spi_id=1.
;   4) Poll DMA_STATUS busy (bit 0) until cleared.
;   5) Drive CS high, ccached, OR-reduce dst into r15.
;
; Expected = OR of bytes {0x10, 0x11, ..., 0x2F} interpreted as 8 LE
; 32-bit words.
;   word0 = 0x13121110, word1 = 0x17161514, word2 = 0x1B1A1918,
;   word3 = 0x1F1E1D1C, word4 = 0x23222120, word5 = 0x27262524,
;   word6 = 0x2B2A2928, word7 = 0x2F2E2D2C
; OR-reduce = 0x3F3E3D3C = 1061043516
;
; expected=1061043516

Main:
    load32 0x2000 r2          ; dst byte addr (line-aligned)
    load 0 r4                 ; sentinel for dst init

    ; --- 1) Initialise dst region with sentinel ---
    write 0  r2 r4
    write 4  r2 r4
    write 8  r2 r4
    write 12 r2 r4
    write 16 r2 r4
    write 20 r2 r4
    write 24 r2 r4
    write 28 r2 r4
    ccached

    ; --- 2) Drive SPI1 CS low (the engine doesn't manage CS) ---
    load32 0x1C00002C r6      ; ADDR_SPI1_CS
    write 0 r6 r0             ; CS = 0 (select)

    ; --- 3) Program the DMA engine via MMIO ---
    load32 0x1C000070 r8      ; DMA register block base
    write 0  r8 r0            ; DMA_SRC   = 0 (ignored for SPI2MEM_QSPI)
    write 4  r8 r2            ; DMA_DST   = 0x2000
    load 32 r9
    write 8  r8 r9            ; DMA_COUNT = 32
    load 16 r10
    ; DMA_QSPI_ADDR is at offset 0x14 from DMA_SRC base (reg_addr=5).
    ; DMA register layout:
    ;   0x00 SRC, 0x04 DST, 0x08 COUNT, 0x0C CTRL, 0x10 STATUS, 0x14 QSPI_ADDR
    write 20 r8 r10           ; DMA_QSPI_ADDR = 0x10

    ; CTRL: bit 31 = start, bits[7:5] = spi_id (1 = SPI1),
    ;       bits[3:0] = mode (6 = SPI2MEM_QSPI)
    ;   = 0x80000000 | (1 << 5) | 6 = 0x80000026
    load32 0x80000026 r11
    write 12 r8 r11           ; DMA_CTRL = start | SPI2MEM_QSPI | spi_id=1

    ; --- 4) Poll DMA_STATUS until busy bit clears ---
    load 1 r12                ; busy mask
PollBusy:
    read 16 r8 r13            ; r13 = STATUS
    and r13 r12 r14
    bne r14 r0 PollBusy

    ; --- 5) CS = 1 (deselect) ---
    load 1 r14
    write 0 r6 r14

    ccached

    ; --- 6) OR-reduce all 8 dst words into r15 ---
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
    or r14 r13 r15            ; expected: 0x3F3E3D3C

    halt
