; Simple program to test timer1
Main:

    load32 0x7000000 r1 ; MU base address
    load 0 r15 ; Clear timer1 interrupt trigger value

    load32 5000 r2 ; Load timer1 value (in ms)
    write 2 r1 r2 ; Set timer1 value
    load 1 r3 ; Load timer1 control value and compare value
    write 3 r1 r3 ; Start timer1

    WaitTimer:
        beq r15 r3 2 ; If timer1 interrupt triggered, skip next instruction
        jump WaitTimer

    
    load 0x42 r4
    write 0 r1 r4


    halt


; Ignore interrupts
Int:
    readintid r13
    load 2 r14
    
    beq r13 r14 2 ; If timer1 interrupt, skip next instruction
    reti ; Not timer1 interrupt, return

    load 0x43 r4
    write 0 r1 r4

    ; Timer1 interrupt
    load 1 r15
    reti
