Main:
    ; Only offsets are tested because static address depends on ROM placement in memory map
    load 4 r1
    jumpro -1 r1
    load 1 r15
    load 2 r15
    load 3 r15
    load 4 r15
    load 5 r15
    load 6 r15
    load 7 r15
    halt

