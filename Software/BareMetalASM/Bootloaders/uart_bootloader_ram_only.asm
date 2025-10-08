; UART Bootloader to be run from RAM
; Requires special header to halt at address 0
; Also requires all registers to be 0 before execution
; Halts at PC0, after which it waits for UART RX interrupts to receive data and write to RAM
; Works by making use of l1i cache to keep halting at 0 until a ccache instruction is used after coying the entire program
; First, 4 bytes are received to determine the size of the program to be copied, after which the length is sent back to confirm
; Then, the program is received byte by byte and written per word to RAM starting at address 0
; Finally, a 'd' is sent to confirm the program has been received and execution is started by clearing the cache, registers and returning from the interrupt
; This bootloader stops working if its code is evicted from l1i cache
;  therefore, I'll most likely need to move the code to a higher RAM address

; Certain registers are used in this program:
; r15: length of program to be received (in words)
; r14: current RAM address to write to (in bytes)
; r13: current word being received
; r12: byte shift counter

; r10: length of program is received flag

; r3: temporary
; r2: temporary
; r1: temporary

Main:
    ; Uncomment the below code to test receiving data in simulation as RX is connected to TX

    ; load32 0x7000000 r6
    ; load 0 r7

    ; ; Send length
    ; write 0 r6 r7
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop

    ; write 0 r6 r7
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop

    ; write 0 r6 r7
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop

    ; load 10 r7
    ; write 0 r6 r7
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop

    ; ; Send word 0
    ; load32 0b10010000000000000000000000000110 r7
    ; shiftr r7 24 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 16 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 8 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 0 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop

    ; ; Send word 1
    ; load32 0b10010000000000000000000000101000 r7
    ; shiftr r7 24 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 16 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 8 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop
    ; shiftr r7 0 r8
    ; write 0 r6 r8
    ; nop
    ; nop
    ; jumpo 2 ; force jump to trigger interrupt
    ; nop
    ; nop


    halt


ReceiveLength:

    ; Receive the byte from UART
    load32 0x7000000 r3 ; UART tx address
    read 1 r3 r1        ; Read data from UART (tx + 1 = rx)

    ; Check bitshift
    load 0 r2
    bne r2 r12 2
        shiftl r1 24 r1 ; First byte, shift to highest byte

    load 1 r2
    bne r2 r12 2
        shiftl r1 16 r1 ; Second byte, shift to second highest byte
    
    load 2 r2
    bne r2 r12 2
        shiftl r1 8 r1 ; Third byte, shift to second lowest byte

    ; Fourth byte, no shift needed

    add r15 r1 r15 ; Add to length
    add r12 1 r12 ; Increment byte shift counter

    ; Check if 4 bytes received
    load 4 r2
    beq r12 r2 2
        reti ; Not done yet, wait for next byte

    ;Length received, send back to confirm
    shiftr r15 24 r1 ; Get highest byte
    write 0 r3 r1 ; Send byte
    shiftr r15 16 r1 ; Get second highest byte
    write 0 r3 r1 ; Send byte
    shiftr r15 8 r1 ; Get second lowest byte
    write 0 r3 r1 ; Send byte
    shiftr r15 0 r1 ; Get lowest byte
    write 0 r3 r1 ; Send byte

    load 1 r10 ; Set length received flag
    load 0 r12 ; Reset byte shift counter

    reti ; Return from interrupt, wait for next byte


ReceiveProgram:

    ; Receive the byte from UART
    load32 0x7000000 r3 ; UART tx address
    read 1 r3 r1        ; Read data from UART (tx + 1 = rx)

    ; Check bitshift
    load 0 r2
    bne r2 r12 2
        shiftl r1 24 r1 ; First byte, shift to highest byte

    load 1 r2
    bne r2 r12 2
        shiftl r1 16 r1 ; Second byte, shift to second highest byte
    
    load 2 r2
    bne r2 r12 2
        shiftl r1 8 r1 ; Third byte, shift to second lowest byte

    ; Fourth byte, no shift needed

    add r13 r1 r13 ; Add to current word
    add r12 1 r12 ; Increment byte shift counter

    ; Check if 4 bytes received
    load 4 r2
    beq r12 r2 2
        reti ; Not done yet, wait for next byte

    ; Word received, write to RAM
    write 0 r14 r13 ; Write word to RAM
    add r14 1 r14 ; Increment RAM address

    load 0 r13 ; Reset current word
    load 0 r12 ; Reset byte shift counter

    ; Check if entire program received
    beq r14 r15 2
        reti ; Not done yet, wait for next byte

    ; Entire program received, send 'd' to confirm
    load 0x64 r1 ; ASCII 'd'
    write 0 r3 r1 ; Send byte

    ; Make ready for execution
    load 0 r1
    load 0 r2
    load 0 r3
    load 0 r4
    load 0 r5
    load 0 r6
    load 0 r7
    load 0 r8
    load 0 r9
    load 0 r10
    load 0 r11
    load 0 r12
    load 0 r13
    load 0 r14
    load 0 r15
    nop
    ccache ; Clear cache
    reti
    reti

Int:
    ; Check interrupt ID to be 1 (UART RX)
    readintid r1
    load 1 r2
    beq r1 r2 2
        reti ; Not UART RX interrupt, return

    ; Check if length of program is received
    beq r2 r10 2
        jump ReceiveLength ; Length not received yet, receive it first
    
    ; Length received, receive program
    jump ReceiveProgram

    ; Although not needed, add nops to avoid potential issues with pipeline
    nop
    nop
    nop
