; Test: Camera status register initial value
;
; CAM_STATUS (0x1C00008C): should read 4 initially
;   bit 0 = frame_done_latch = 0
;   bit 1 = current_buf = 0
;   bit 2 = 1 (was cam_configure_done, now hardwired)
;   bits 3-4 = vsync/href raw = 0
;
; Result: r15 = CAM_STATUS = 4
;
; expected=4

Main:
    ; --- CAM_STATUS: read initial state ---
    load32 0x1C00008C r5        ; ADDR_CAM_STATUS
    read 0 r5 r15              ; r15 = {27'b0, href, vsync, 1, cur_buf, frame_done}
                                ; Should be 4 (bit2=1, rest=0)

    halt
