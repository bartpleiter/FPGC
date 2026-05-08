; Test: Camera buffer default addresses
;
; Verifies that CAM_BUF0 and CAM_BUF1 power-on defaults are correct.
; MemoryUnit initializes:
;   CAM_BUF0 = 0x100000 (21-bit line address = byte addr 0x02000000)
;   CAM_BUF1 = 0x100960 (buf0 + 2400 lines = byte addr 0x02012C00)
;
; Result: r15 = CAM_BUF0 + CAM_BUF1 = 0x100000 + 0x100960 = 0x200960
;       = 2099552
;
; expected=2099552

Main:
    ; --- Read default CAM_BUF0 ---
    load32 0x1C000094 r5        ; ADDR_CAM_BUF0
    read 0 r5 r10              ; r10 = default buf0 addr (0x07E000)

    ; --- Read default CAM_BUF1 ---
    load32 0x1C000098 r5        ; ADDR_CAM_BUF1
    read 0 r5 r11              ; r11 = default buf1 addr (0x07E960)

    ; --- Combine ---
    add r10 r11 r15            ; r15 = 0x07E000 + 0x07E960 = 0x0FC960

    halt
