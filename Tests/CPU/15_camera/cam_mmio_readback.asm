; Test: Camera MMIO register readback — CAM_CTRL, CAM_DBG0, CAM_DBG1, CAM_DBG2
;
; Writes enable=1 to CAM_CTRL, reads it back.
; Reads camera debug registers (all zero at startup, no frame captured).
;
; CAM_CTRL   (0x1C000088): write enable=1, read back bit[0] = 1
; CAM_DBG0   (0x1C000094): read-only, = 0 (no frame yet)
; CAM_DBG1   (0x1C000098): read-only, = 0 (no frame yet)
; CAM_DBG2   (0x1C00009C): read-only, [2:0]=state, [3]=arb_busy
;                          state=IDLE(0), arb_busy=0 in sim → 0
;
; Result: r15 = CAM_CTRL[0] + DBG0 + DBG1 + DBG2 = 1 + 0 + 0 + 0 = 1
;
; expected=1

Main:
    ; --- CAM_CTRL: write enable=1, read back ---
    load32 0x1C000088 r5        ; ADDR_CAM_CTRL
    load 1 r1
    write 0 r5 r1              ; enable capture
    read 0 r5 r10              ; r10 = CAM_CTRL readback (should be 1)

    ; --- CAM_DBG0 (was CAM_BUF0): read-only debug ---
    load32 0x1C000094 r5        ; ADDR_CAM_DBG0
    read 0 r5 r11              ; r11 = 0 (no frame)

    ; --- CAM_DBG1 (was CAM_BUF1): read-only debug ---
    load32 0x1C000098 r5        ; ADDR_CAM_DBG1
    read 0 r5 r12              ; r12 = 0 (no frame)

    ; --- CAM_DBG2: state + arb_busy ---
    load32 0x1C00009C r5        ; ADDR_CAM_DBG2
    read 0 r5 r13              ; r13 = 0 (idle, not busy)

    ; --- Combine result ---
    add r10 r11 r15
    add r15 r12 r15
    add r15 r13 r15            ; r15 = 1 + 0 + 0 + 0 = 1

    halt
