Main:
    load 1 r1
    load 5 r2
    add r1 1 r1
    add r1 1 r1
    beq r1 r2 Done
    jumpo -3
    load 1 r15 ; should not be reached
    halt
Done:
    load 7 r15 ; expected=7
    halt
