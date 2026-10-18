; Test: Signed Division (DIVS) and Signed Modulo (MODS)
; Tests signed division and modulo with positive and negative numbers

Main:
    ; Test 1: 20 / 4 = 5 (positive / positive)
    load 20 r1
    load 4 r2
    divs r1 r2 r3       ; r3 = 5

    ; Test 2: -20 / 4 = -5 (negative / positive)
    load32 -20 r4
    divs r4 r2 r5       ; r5 = -5 (0xFFFFFFFB)

    ; Test 3: 20 / -4 = -5 (positive / negative)
    load32 -4 r6
    divs r1 r6 r7       ; r7 = -5 (0xFFFFFFFB)

    ; Test 4: -20 / -4 = 5 (negative / negative)
    divs r4 r6 r8       ; r8 = 5

    ; Test 5: -17 % 5 = -2 (remainder has sign of dividend)
    load32 -17 r9
    load 5 r10
    mods r9 r10 r11     ; r11 = -2 (0xFFFFFFFE)

    ; Test 6: 17 % -5 = 2 (remainder has sign of dividend)
    load 17 r12
    load32 -5 r13
    mods r12 r13 r14    ; r14 = 2
    
    ; Verify results:
    ; Check r3=5, r8=5: 5+5=10
    add r3 r8 r15       ; expected=10

    halt
