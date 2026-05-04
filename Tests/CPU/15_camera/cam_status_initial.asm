; Test: Camera status register and SCCB ready
;
; CAM_STATUS (0x1C00008C): should read 0 initially (no frame_done, buf=0)
; CAM_SCCB   (0x1C000090): read should return 1 (ready=1, no SCCB in flight)
;
; Result: r15 = CAM_STATUS + CAM_SCCB_ready = 0 + 1 = 1
;
; expected=1

Main:
    ; --- CAM_STATUS: read initial state ---
    load32 0x1C00008C r5        ; ADDR_CAM_STATUS
    read 0 r5 r10              ; r10 = {30'b0, current_buf, frame_done_latch}
                                ; Should be 0 (no frames captured, buf=0)

    ; --- CAM_SCCB: read ready state ---
    load32 0x1C000090 r5        ; ADDR_CAM_SCCB
    read 0 r5 r11              ; r11 = SCCB ready (should be 1)

    ; --- Combine ---
    add r10 r11 r15            ; r15 = 0 + 1 = 1

    halt
