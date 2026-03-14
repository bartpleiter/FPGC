Main:
    load 7 r1
    savpc r2
    jumpo 20
    halt
    load 7 r15 ; expected=7
    halt
    halt
    jumpr 12 r2
    halt
