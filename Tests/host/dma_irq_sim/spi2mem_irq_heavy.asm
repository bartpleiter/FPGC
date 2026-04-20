; Test: DMA SPI2MEM under repeated IRQ injection — heavy ISR variant.
;
; Like spi2mem_irq_stress.asm but the ISR mimics crt0_baremetal: it
; pushes 15 registers onto the hardware Stack, performs MMIO reads
; (readintid, plus reads from MU registers), then pops and RETIs.
; The intent is to maximise the number of pipeline events between the
; CPU's MMIO-load (dma_status() poll) and the IRQ entry/exit, and to
; stress the cpu_req_pending / mu_done handshake.
;
; expected=4

    jump Start
    jump IntH

Start:
    load32 0x1C000020 r5      ; ADDR_SPI0_DATA
    load32 0x1C000024 r6      ; ADDR_SPI0_CS
    load32 0x1C000070 r8      ; DMA register block base
    load 0 r9                 ; iteration counter
    load 4 r10                ; N iterations (smaller because DMA is bigger)

LoopTop:
    write 0 r6 r0             ; CS = 0
    load 3 r7                 ; READ_DATA (0x03)
    write 0 r5 r7
    write 0 r5 r0
    write 0 r5 r0
    write 0 r5 r0

    write 0  r8 r0            ; DMA_SRC = 0
    load32 0x10000 r1
    write 4  r8 r1            ; DMA_DST = 0x10000 (above code in SDRAM mode)
    load 256 r2               ; 256-byte DMA (longer poll window)
    write 8  r8 r2            ; DMA_COUNT
    load32 0x80000002 r3      ; CTRL: start | SPI2MEM | spi_id=0
    write 12 r8 r3            ; DMA_CTRL

    load 1 r11
PollBusy:
    read 16 r8 r12            ; STATUS (target of IRQ races)
    and r12 r11 r13
    bne r13 r0 PollBusy

    load 1 r4
    write 0 r6 r4
    ccached

    add r9 1 r9
    bne r9 r10 LoopTop

    or r9 r0 r15
    halt

; ===== Heavy interrupt handler: mimics crt0_baremetal =====
IntH:
    push r1
    push r2
    push r3
    push r4
    push r5
    push r6
    push r7
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Body: read int ID, then read DMA_STATUS (also MMIO).
    readintid r1
    load32 0x1C000070 r2
    read 16 r2 r3             ; DMA_STATUS
    read 16 r2 r3             ; again
    load32 0x1C000010 r4      ; UART status reg-ish
    read 0 r4 r5

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop r7
    pop r6
    pop r5
    pop r4
    pop r3
    pop r2
    pop r1
    reti
