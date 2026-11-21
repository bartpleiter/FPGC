; Echoes three bytes in upper case from the UART's interrupt (2). The handler reads
; until nothing's waiting. The main loop waits until the handler has seen three bytes.
; runs from SDRAM
; devices
; uart input "xyz" at cycle 1500
; uart output "XYZ"
    jump Main
    jump Interrupt
Interrupt:
    readintid r5
Next:
    read 0xFFFF8004 r0 r1       ; RECEIVE
    sub r1 32 r1
    write 0xFFFF8000 r0 r1      ; TRANSMIT
    add r6 1 r6
    read 0xFFFF8008 r0 r2       ; STATUS
    and r2 1 r2
    bne r2 r0 Next
    reti
Main:
    load 0 r6
    load 3 r7
    load 1 r1
    write 0xFFFF800C r0 r1      ; CONTROL: the interrupt
Wait:
    bne r6 r7 Wait
    halt
