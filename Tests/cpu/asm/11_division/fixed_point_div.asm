; Test: Fixed-Point Division (DIVFP)
; Tests fixed-point division using Q16.16 format
; In Q16.16: integer part in upper 16 bits, fractional in lower 16 bits
; Value = raw_value / 65536

Main:
    ; Test 1: 2.0 / 0.5 = 4.0
    ; 2.0 in Q16.16 = 2 * 65536 = 131072 (0x00020000)
    ; 0.5 in Q16.16 = 0.5 * 65536 = 32768 (0x00008000)
    ; Expected: 4.0 in Q16.16 = 4 * 65536 = 262144 (0x00040000)
    load32 0x00020000 r1    ; 2.0
    load32 0x00008000 r2    ; 0.5
    divfp r1 r2 r3          ; r3 = 4.0 (0x00040000)

    ; Test 2: 1.0 / 4.0 = 0.25
    ; 1.0 in Q16.16 = 65536 (0x00010000)
    ; 4.0 in Q16.16 = 262144 (0x00040000)
    ; Expected: 0.25 in Q16.16 = 16384 (0x00004000)
    load32 0x00010000 r4    ; 1.0
    load32 0x00040000 r5    ; 4.0
    divfp r4 r5 r6          ; r6 = 0.25 (0x00004000)

    ; Test 3: 3.0 / 2.0 = 1.5
    ; 3.0 in Q16.16 = 196608 (0x00030000)
    ; 2.0 in Q16.16 = 131072 (0x00020000)
    ; Expected: 1.5 in Q16.16 = 98304 (0x00018000)
    load32 0x00030000 r7    ; 3.0
    divfp r7 r1 r8          ; r8 = 1.5 (0x00018000)

    ; Verify: extract integer parts and sum
    ; 4.0 >> 16 = 4
    ; 0.25 >> 16 = 0
    ; 1.5 >> 16 = 1
    ; Sum = 5
    shiftr r3 16 r9         ; r9 = 4
    shiftr r6 16 r10        ; r10 = 0
    shiftr r8 16 r11        ; r11 = 1
    add r9 r10 r12
    add r12 r11 r15         ; expected=5

    halt
