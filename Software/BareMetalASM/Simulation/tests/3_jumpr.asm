Main:
    ; Only offsets are tested because static address depends on ROM placement in memory map
    load 4 r1
    jumpro -1 r1
    load 3 r15
    halt
    load 7 r15 ; expected=7
    halt

