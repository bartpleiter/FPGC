; Test: Camera MMIO register write/read — CAM_CTRL, CAM_BUF0, CAM_BUF1
;
; Writes values to camera control registers and reads them back.
; Verifies the registers are correctly implemented in MemoryUnit.
;
; CAM_CTRL   (0x1C000088): write enable=1, read back bit[0]
; CAM_BUF0   (0x1C000094): write 0x012345, read back
; CAM_BUF1   (0x1C000098): write 0x067890, read back
;
; Result: r15 = CAM_CTRL[0] + CAM_BUF0 + CAM_BUF1
;       = 1 + 0x012345 + 0x067890 = 0x079BD6 = 498646
;
; expected=498646

Main:
    ; --- CAM_CTRL: write enable=1, read back ---
    load32 0x1C000088 r5        ; ADDR_CAM_CTRL
    load 1 r1
    write 0 r5 r1              ; enable capture
    read 0 r5 r10              ; r10 = CAM_CTRL readback (should be 1)

    ; --- CAM_BUF0: write custom base, read back ---
    load32 0x1C000094 r5        ; ADDR_CAM_BUF0
    load32 0x012345 r1
    write 0 r5 r1
    read 0 r5 r11              ; r11 = CAM_BUF0 readback (should be 0x012345)

    ; --- CAM_BUF1: write custom base, read back ---
    load32 0x1C000098 r5        ; ADDR_CAM_BUF1
    load32 0x067890 r1
    write 0 r5 r1
    read 0 r5 r12              ; r12 = CAM_BUF1 readback (should be 0x067890)

    ; --- Combine result ---
    add r10 r11 r15
    add r15 r12 r15            ; r15 = 1 + 0x012345 + 0x067890 = 0x079BD6

    halt
