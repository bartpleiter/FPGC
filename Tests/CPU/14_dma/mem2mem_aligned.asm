; Test: DMA MEM2MEM aligned 32-byte copy
;
; Verifies the DMAengine (step 8) can copy a single 32-byte cache line from
; SDRAM region A to SDRAM region B. Uses ccached around the DMA so that
;   - the dirty pattern in L1d is flushed to SDRAM before the engine reads it
;   - the L1d is invalidated after the DMA so the readback fetches from SDRAM
;
; If the engine works:
;   - dst region holds 8 copies of the pattern after the DMA completes
;   - OR-reduce of all 8 dst words equals the pattern
;
; expected=3735928559

Main:
    load32 0x1000 r1          ; src byte addr (line-aligned, well past test code)
    load32 0x2000 r2          ; dst byte addr (line-aligned)
    load32 0xDEADBEEF r3      ; pattern
    load 0 r4                 ; sentinel for dst init

    ; --- 1) Initialise src region with pattern (8 words = 32 bytes) ---
    write 0  r1 r3
    write 4  r1 r3
    write 8  r1 r3
    write 12 r1 r3
    write 16 r1 r3
    write 20 r1 r3
    write 24 r1 r3
    write 28 r1 r3

    ; --- 2) Initialise dst region with sentinel (so a no-op DMA would fail) ---
    write 0  r2 r4
    write 4  r2 r4
    write 8  r2 r4
    write 12 r2 r4
    write 16 r2 r4
    write 20 r2 r4
    write 24 r2 r4
    write 28 r2 r4

    ; --- 3) Flush L1d so SDRAM has the dirty data the engine will read ---
    ccached

    ; --- 4) Program the DMA engine via MMIO ---
    load32 0x1C000070 r5      ; DMA register block base (SRC)
    write 0  r5 r1            ; DMA_SRC   = 0x1000
    write 4  r5 r2            ; DMA_DST   = 0x2000
    load 32 r6
    write 8  r5 r6            ; DMA_COUNT = 32
    load32 0x80000000 r7      ; CTRL: start=1, mode=MEM2MEM(0), irq_en=0
    write 12 r5 r7            ; DMA_CTRL  = start | MEM2MEM

    ; --- 5) Poll DMA_STATUS until busy bit (bit 0) clears ---
    load 1 r8                 ; busy mask
PollBusy:
    read 16 r5 r9             ; r9 = STATUS (offset 0x10 from SRC base)
    and r9 r8 r10
    bne r10 r0 PollBusy

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
    or r14 r13 r15            ; expected: 0xDEADBEEF (= -559038737 signed)

    halt
