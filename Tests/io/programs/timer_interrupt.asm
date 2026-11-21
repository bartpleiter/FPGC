; The timer interrupts (1) every 500 cycles, and the handler prints a dot. After four,
; the program stops the timer and reads how many cycles went by on the cycle counter:
; a little over 2000.
; runs from SDRAM
; devices
; uart output "....!"
    jump Main
    jump Interrupt
Interrupt:
    readintid r5
    write 0xFFFF8208 r0 r0      ; clear the timer's flag
    load 0x2E r1
    write 0xFFFF8000 r0 r1      ; TRANSMIT
    add r6 1 r6
    reti
Main:
    load 0 r6
    load 4 r7
    write 0xFFFF8100 r0 r0      ; a snapshot of the cycle counter
    load 3 r1
    write 0xFFFF8204 r0 r1      ; CONTROL: periodic, the interrupt
    load 500 r1
    write 0xFFFF8200 r0 r1      ; COUNT
    read 0xFFFF8104 r0 r10      ; the snapshot's low word
Wait:
    bne r6 r7 Wait
    write 0xFFFF8200 r0 r0      ; stop
    write 0xFFFF8100 r0 r0
    read 0xFFFF8104 r0 r8
    read 0xFFFF8108 r0 r9       ; the high word: 0
    sub r8 r10 r8               ; the cycles
    load 0x21 r1
    write 0xFFFF8000 r0 r1
    halt
