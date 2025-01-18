; Simple program to test some basic pipeline hazards detections and other functionality

Main:
    load 5 r1           ; r1:=5
    load 2 r2           ; r2:=2
    load32 0x1200000 r4
    add r1 r2 r3        ; r3:=7
    add r3 r2 r3        ; r3:=9
    add r1 r3 r3        ; r3:=14
    add r3 r3 r3        ; r3:=28
    write 10 r4 r3      ; mem(VRAM8 + 10):=28
    write 0 r4 r1       ; mem(VRAM8):=5
    read 10 r4 r2       ; r4:=28
    load 9 r9
    load 10 r10
    load 11 r11
    halt


Int:
    reti
