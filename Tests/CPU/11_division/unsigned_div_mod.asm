; Test: Unsigned Division (DIVU) and Unsigned Modulo (MODU)
; Tests basic unsigned division and modulo operations

Main:
    ; Test 1: 28 / 4 = 7
    load 28 r1
    load 4 r2
    divu r1 r2 r3       ; r3 = 7

    ; Test 2: 28 % 4 = 0
    modu r1 r2 r4       ; r4 = 0

    ; Test 3: 29 / 4 = 7
    load 29 r5
    divu r5 r2 r6       ; r6 = 7

    ; Test 4: 29 % 4 = 1
    modu r5 r2 r7       ; r7 = 1

    ; Test 5: 100 / 7 = 14
    load 100 r8
    load 7 r9
    divu r8 r9 r10      ; r10 = 14

    ; Test 6: 100 % 7 = 2
    modu r8 r9 r11      ; r11 = 2

    ; Verify results: 7 + 0 + 7 + 1 + 14 + 2 = 31
    add r3 r4 r12
    add r12 r6 r12
    add r12 r7 r12
    add r12 r10 r12
    add r12 r11 r15     ; expected=31

    halt
