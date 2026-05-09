; Test: Camera debug registers initial values
;
; Verifies that the camera debug registers (which replaced CAM_BUF0/BUF1)
; read as zero at power-on (no frame captured yet).
;   CAM_DBG0 (0x94) = {line_count[8:0], frame_pixels[16:0]} = 0
;   CAM_DBG1 (0x98) = {partial_drops[7:0], cache_lines[11:0]} = 0
;
; Result: r15 = CAM_DBG0 + CAM_DBG1 = 0 + 0 = 0
;
; expected=0

Main:
    ; --- Read CAM_DBG0 (was CAM_BUF0) ---
    load32 0x1C000094 r5        ; ADDR_CAM_DBG0
    read 0 r5 r10              ; r10 = {line_count, frame_pixels} = 0

    ; --- Read CAM_DBG1 (was CAM_BUF1) ---
    load32 0x1C000098 r5        ; ADDR_CAM_DBG1
    read 0 r5 r11              ; r11 = {partial_drops, cache_lines} = 0

    ; --- Combine ---
    add r10 r11 r15            ; r15 = 0 + 0 = 0

    halt
