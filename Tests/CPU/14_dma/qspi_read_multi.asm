; Test: DMA SPI2MEM_QSPI multi-line Fast Read on SPI1 (64 bytes / 2 lines)
;
; Exercises the iter-5b single-burst-multi-line QSPI path in DMAengine:
; one QSPIflash burst of 64 bytes is kicked off, the engine drains
; 32 bytes into SDRAM, returns to ST_S2M_BURST (skips kickoff because
; qspi_burst_open=1) and waits for another 32-byte line in the FIFO.
;
; qspi_flash_sim drives data[i] = (start_addr + i) & 0xFF. With
; start_addr = 0x10 the 64 bytes ramp from 0x10 .. 0x4F.
;
; Words (LE 4-byte little-endian):
;   line 0 (offset 0..31) ramps 0x10..0x2F -- OR = 0x3F3E3D3C
;   line 1 (offset 32..63) ramps 0x30..0x4F -- OR = 0x7F7E7D7C
; Final OR-reduce of all 16 words = 0x7F7E7D7C
;
; expected=2138996092

Main:
    load32 0x2000 r2          ; dst byte addr (line-aligned)

    ; --- Sentinel-fill the 64-byte dst region ---
    write 0  r2 r0
    write 4  r2 r0
    write 8  r2 r0
    write 12 r2 r0
    write 16 r2 r0
    write 20 r2 r0
    write 24 r2 r0
    write 28 r2 r0
    write 32 r2 r0
    write 36 r2 r0
    write 40 r2 r0
    write 44 r2 r0
    write 48 r2 r0
    write 52 r2 r0
    write 56 r2 r0
    write 60 r2 r0
    ccached

    ; --- Drive SPI1 CS low ---
    load32 0x1C00002C r6
    write 0 r6 r0

    ; --- Program the DMA engine ---
    load32 0x1C000070 r8
    write 0  r8 r0            ; SRC unused
    write 4  r8 r2            ; DST = 0x2000
    load 64 r9
    write 8  r8 r9            ; COUNT = 64
    load 16 r10
    write 20 r8 r10           ; QSPI_ADDR = 0x10

    load32 0x80000026 r11     ; start | mode=6 | spi_id=1
    write 12 r8 r11

    ; --- Poll DMA_STATUS until busy bit clears ---
    load 1 r12
PollBusy:
    read 16 r8 r13
    and r13 r12 r14
    bne r14 r0 PollBusy

    ; --- CS high ---
    load 1 r14
    write 0 r6 r14

    ccached

    ; --- OR-reduce all 16 dst words into r15 ---
    read 0  r2 r10
    read 4  r2 r11
    or r10 r11 r15
    read 8  r2 r10
    or r15 r10 r15
    read 12 r2 r10
    or r15 r10 r15
    read 16 r2 r10
    or r15 r10 r15
    read 20 r2 r10
    or r15 r10 r15
    read 24 r2 r10
    or r15 r10 r15
    read 28 r2 r10
    or r15 r10 r15
    read 32 r2 r10
    or r15 r10 r15
    read 36 r2 r10
    or r15 r10 r15
    read 40 r2 r10
    or r15 r10 r15
    read 44 r2 r10
    or r15 r10 r15
    read 48 r2 r10
    or r15 r10 r15
    read 52 r2 r10
    or r15 r10 r15
    read 56 r2 r10
    or r15 r10 r15
    read 60 r2 r10
    or r15 r10 r15

    halt
