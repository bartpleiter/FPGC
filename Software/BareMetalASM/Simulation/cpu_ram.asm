Main:
    load 7 r1                   ; pc0
    savpc r2                    ; pc1
    jumpo 5                     ; pc2
    load 999 r9                        ; pc3
    load 7 r15 ; expected=7     ; pc4
    halt                        ; pc5
    load 999 r9                        ; pc6
    jumpr 3 r2                  ; pc7

    load 999 r9                        ; pc8
    halt                               ; pc9

; Expected execution order:
; pc0 -> pc1 -> pc2 -> pc7 -> pc4 -> pc5 -> pc5 -> pc5...
