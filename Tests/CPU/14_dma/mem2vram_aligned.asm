; Test: DMA MEM2VRAM aligned 32-byte blit
;
; Verifies the DMAengine MEM2VRAM mode (Phase 1, step 14):
;   - SDRAM source is read 32 bytes at a time;
;   - the line is drained byte-by-byte into VRAMPX via the vp_* port;
;   - the engine completes cleanly with sticky_done=1 and error=0.
;
; VRAMPX is write-only from the CPU side, so we cannot read the destination
; back. Instead we check the engine's STATUS register: after a successful
; transfer, reading STATUS once must return {error=0, done=1, busy=0} = 2.
; A second STATUS read returns 0 because the sticky bits self-clear on read.
;
; Then we exercise the alignment-validation path: a transfer with count=33
; (not a multiple of 32) must complete immediately with error=1, so STATUS
; reads back as 4 (sticky_error=1, sticky_done=0, busy=0).
;
; r15 = (good_status << 4) | bad_status = 0x24 = 36.
;
; expected=36

Main:
    load32 0x1000 r1          ; src byte addr (line-aligned, well past test code)
    load32 0xCAFEBABE r3      ; pattern

    ; --- 1) Initialise src region with pattern (8 words = 32 bytes) ---
    write 0  r1 r3
    write 4  r1 r3
    write 8  r1 r3
    write 12 r1 r3
    write 16 r1 r3
    write 20 r1 r3
    write 24 r1 r3
    write 28 r1 r3

    ; --- 2) Flush L1d so SDRAM has the dirty bytes the engine will read ---
    ccached

    ; --- 3) Program the DMA engine: MEM2VRAM, src=0x1000, dst=0x1EC00000, count=32 ---
    load32 0x1C000070 r5      ; DMA register block base (SRC)
    write 0  r5 r1            ; DMA_SRC   = 0x1000
    load32 0x1EC00000 r2      ; VRAMPX base
    write 4  r5 r2            ; DMA_DST   = 0x1EC00000
    load 32 r6
    write 8  r5 r6            ; DMA_COUNT = 32
    load32 0x80000003 r7      ; CTRL: start=1, mode=MEM2VRAM(3), irq_en=0
    write 12 r5 r7            ; DMA_CTRL  = start | MEM2VRAM

    ; --- 4) Poll DMA_STATUS until busy bit (bit 0) clears ---
    ; Note: every STATUS read clears the sticky done/error bits, so the
    ; iteration that observes busy=0 must capture the full status value
    ; for inspection -- a follow-up `read` would return 0.
    load 1 r8                 ; busy mask
PollGood:
    read 16 r5 r9             ; STATUS (offset 0x10 from SRC base)
    and r9 r8 r10
    bne r10 r0 PollGood

    ; --- 5) r9 holds the good STATUS (done=1, error=0, busy=0) = 2 ---
    or r9 r0 r12

    ; --- 6) Now trigger an alignment error: count=33 with same dst ---
    load 33 r6
    write 8  r5 r6            ; DMA_COUNT = 33 (not 32-aligned)
    write 0  r5 r1            ; DMA_SRC   = 0x1000 (defensive)
    write 4  r5 r2            ; DMA_DST   = 0x1EC00000
    load32 0x80000003 r7
    write 12 r5 r7            ; CTRL: start | MEM2VRAM

    ; --- 7) Poll busy until cleared; r9 latches the good last-read value ---
PollBad:
    read 16 r5 r9
    and r9 r8 r10
    bne r10 r0 PollBad

    ; --- 8) r9 holds the bad STATUS (done=1, error=1, busy=0) = 3 ---
    or r9 r0 r13

    ; --- 9) Pack: r15 = (good << 4) | bad = 0x23 = 35 ---
    shiftl r12 4 r12
    or r12 r13 r15

    halt
