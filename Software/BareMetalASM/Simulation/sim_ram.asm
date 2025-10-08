; Simple program to test timer1
Main:

    load32 0x7000000 r1 ; MU base address


    load 1 r3 ; Load timer1 control value and compare value
    write 3 r1 r3 ; Start timer1

    WaitTimer:
        beq r15 r3 2 ; If timer1 interrupt triggered, skip next instruction
        jump WaitTimer

    
    load 99 r9


    halt


; Ignore interrupts
Int:
    reti
