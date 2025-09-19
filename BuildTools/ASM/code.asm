Main:
    load 7 r1
    savpc r2
    jumpo 5
    halt
    load 7 r15 ; expected=7
    halt
    halt
    jumpr 3 r2
    halt
