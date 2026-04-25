; Test: DMA SPI burst port reaches SPI5 (SD card controller)
;
; Confirms that the DMA engine's spi_id=5 path is wired through MemoryUnit
; to the SimpleSPI2 instance now driving SPI5 (B.5.3 of dma-followups.md).
; The simulation testbenches tie SPI5_miso = 1'b0, so a SPI2MEM read
; against SPI5 must return 32 zero bytes; the test verifies that and that
; DMA STATUS reports done (bit 1 set) with the busy/error bits clear.
;
; expected=2

Main:
    load32 0x2000 r2          ; dst byte addr (line-aligned)

    ; --- 1) Pre-fill dst with a sentinel that's not zero ---
    load32 0xDEADBEEF r4
    write 0  r2 r4
    write 4  r2 r4
    write 8  r2 r4
    write 12 r2 r4
    write 16 r2 r4
    write 20 r2 r4
    write 24 r2 r4
    write 28 r2 r4
    ccached

    ; --- 2) Program DMA: SPI2MEM from SPI5 ---
    load32 0x1C000070 r8      ; DMA register block base (SRC)
    write 0  r8 r0            ; DMA_SRC   = 0 (ignored for SPI2MEM)
    write 4  r8 r2            ; DMA_DST   = 0x2000
    load 32 r9
    write 8  r8 r9            ; DMA_COUNT = 32
    ; CTRL: bit 31=start, bits[7:5]=spi_id (5), bits[3:0]=mode (2=SPI2MEM)
    load32 0x800000A2 r10     ; start | spi_id=5 | SPI2MEM
    write 12 r8 r10

    ; --- 3) Poll busy ---
    load 1 r11
PollBusy:
    read 16 r8 r12            ; STATUS
    and r12 r11 r13
    bne r13 r0 PollBusy

    ccached

    ; --- 4) Confirm all 32 bytes are zero (SPI5_miso tied low in TBs) ---
    read 0  r2 r4
    read 4  r2 r5
    read 8  r2 r6
    read 12 r2 r7
    or r4 r5 r4
    or r4 r6 r4
    or r4 r7 r4
    read 16 r2 r5
    read 20 r2 r6
    read 24 r2 r7
    or r4 r5 r4
    or r4 r6 r4
    or r4 r7 r4
    read 28 r2 r5
    or r4 r5 r4
    bne r4 r0 Fail            ; any non-zero byte means MISO was not seen

    ; --- 5) Confirm DMA STATUS = done (bit1) | !busy (bit0) | !error (bit2) ---
    read 16 r8 r15
    halt

Fail:
    load32 0xBAD r15
    halt
