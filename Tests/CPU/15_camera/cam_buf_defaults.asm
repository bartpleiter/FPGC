; Test: Camera buffer default addresses
;
; Verifies that CAM_BUF0 and CAM_BUF1 power-on defaults are correct.
; MemoryUnit initializes:
;   CAM_BUF0 = 0x1F8000 (21-bit line address = byte addr 0x03F00000)
;   CAM_BUF1 = 0x1F8960 (buf0 + 2400 lines = byte addr 0x03F12C00)
;
; Result: r15 = CAM_BUF0 + CAM_BUF1 = 0x1F8000 + 0x1F8960 = 0x3F0960
;       = 4131168
;
; expected=4131168

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
