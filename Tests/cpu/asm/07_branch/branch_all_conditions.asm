Main:
    load32 -4 r1
    load32 2 r2
    load32 2 r3

    beq r2 r3 8
    load 3 r15 ; will return this in case of incorrect pipeline flush

    bne r1 r2 8
    halt

    bgt r1 r2 8
    halt

    bge r2 r3 8
    halt

    blt r2 r1 8
    halt

    ble r3 r2 8
    halt

    bgts r2 r1 8
    halt

    bges r1 r1 8
    halt

    blts r1 r2 8
    halt

    bles r1 r1 8
    halt

    load 7 r15 ; expected=7
    halt
