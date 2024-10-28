; Simple program to test some basic pipeline hazards detections and other functionality

Main:
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
Main2:
    load 5 r1           ; r1:=5
    load 2 r2           ; r2:=2
    add r1 r2 r3        ; r3:=7
    add r3 r2 r3        ; r3:=9
    add r1 r3 r3        ; r3:=14
    add r3 r3 r3        ; r3:=28
    jump Main2
    write 10 r3 r3      ; mem(10+28):=28
    write 0 r3 r1       ; mem(28):=5
    read 10 r3 r4       ; r4:=28
    add r4 r1 r5        ; r5:=5+28
    bgt r5 r1 3         ; true
    halt                ; skip
    load 0 r1           ; skip
    load 0 r2           ; skip
    load 1 r9           ; r9:=1
    load 18 r10         ; r10:=18
    push r10            ; push 18
    pop r11             ; pop r11:=18
    add r10 r10 r15     ; r15:=36
    savpc r12           ; r12:=pc
    add r12 1 r13       ; r13:=r12+1
    halt                ; halt
    load 37 r1          ; unreached

Int:
    reti
