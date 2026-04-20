; Test: DMA SPI2MEM under repeated IRQ injection.
;
; Stresses the CPU pipeline + MMIO + DMA path under interrupt pressure.
; Designed to be run with the cpu_irq_inject_tb.v testbench, which
; pulses one of the InterruptController inputs every IRQ_PERIOD cycles
; (default ~1667 cycles, mimics FRAME_DRAWN @60Hz).
;
; Sequence per iteration:
;   1) Drive CS=0, send READ_DATA + 24-bit address via single-byte SPI0 MMIO.
;   2) Issue 32-byte DMA SPI2MEM into dst.
;   3) Spin polling DMA_STATUS busy bit.
;   4) CS=1, ccached.
;   5) Increment iteration counter.
; After N iterations, halt with the iteration counter in r15.
;
; Pass: r15 == N (loop completed). Fail: r15 < N (loop wedged).
;
; expected=8

; ===== Boot vectors (PC=0 = jump Start, PC=4 = jump IntH) =====
    jump Start
    jump IntH

Start:
    load32 0x1C000020 r5      ; ADDR_SPI0_DATA
    load32 0x1C000024 r6      ; ADDR_SPI0_CS
    load32 0x1C000070 r8      ; DMA register block base
    load 0 r9                 ; iteration counter (will go to r15 at end)
    load 8 r10                ; N iterations

LoopTop:
    ; --- 1) CS=0, send READ_DATA (0x03) + 24-bit address (0) ---
    write 0 r6 r0             ; CS = 0
    load 3 r7                 ; READ_DATA (0x03)
    write 0 r5 r7             ; spi tx 0x03
    write 0 r5 r0             ; addr[23:16] = 0
    write 0 r5 r0             ; addr[15:8]  = 0
    write 0 r5 r0             ; addr[7:0]   = 0

    ; --- 2) Program DMA: SRC=0, DST=0x2000, COUNT=32, CTRL=start|SPI2MEM|spi_id=0 ---
    write 0  r8 r0            ; DMA_SRC = 0
    load32 0x2000 r1
    write 4  r8 r1            ; DMA_DST = 0x2000
    load 32 r2
    write 8  r8 r2            ; DMA_COUNT = 32
    load32 0x80000002 r3      ; CTRL: start | SPI2MEM | spi_id=0
    write 12 r8 r3            ; DMA_CTRL

    ; --- 3) Poll busy ---
    load 1 r11                ; busy mask
PollBusy:
    read 16 r8 r12            ; STATUS
    and r12 r11 r13
    bne r13 r0 PollBusy

    ; --- 4) CS=1, ccached ---
    load 1 r4
    write 0 r6 r4
    ccached

    ; --- 5) loop ---
    add r9 1 r9
    bne r9 r10 LoopTop

    ; Done — return iteration count
    or r9 r0 r15
    halt

; ===== Interrupt handler: just RETI =====
IntH:
    reti
