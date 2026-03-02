Main:
    ; Test fmul combined with fadd: compute (a*b + c)
    ; a = 3.0, b = 4.0, c = 1.0
    ; 3.0 * 4.0 = 12.0, 12.0 + 1.0 = 13.0

    ; f0 = 3.0
    load 3 r1
    load 0 r2
    fld r1 r2 r0

    ; f1 = 4.0
    load 4 r1
    load 0 r2
    fld r1 r2 r1

    ; f2 = 1.0
    load 1 r1
    load 0 r2
    fld r1 r2 r2

    ; f3 = f0 * f1 = 3.0 * 4.0 = 12.0
    fmul r0 r1 r3

    ; f4 = f3 + f2 = 12.0 + 1.0 = 13.0
    fadd r3 r2 r4

    fsthi r4 r0 r3           ; r3 = 13
    fstlo r4 r0 r4           ; r4 = 0

    add r3 r4 r15            ; expected=13

    halt
