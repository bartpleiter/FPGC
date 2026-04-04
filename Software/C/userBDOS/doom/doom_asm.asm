; doom_asm.asm — Optimized assembly helpers for Doom on FPGC
;
; Provides a fast framebuffer copy routine that replaces the slow
; per-pixel C loop in DG_DrawFrame.

.text

; void doom_draw_frame_asm(unsigned char *src)
;
; Copies 320x200 (64000) bytes from src to the pixel framebuffer at
; 0x1EC00000.  The pixel FB is word-addressed: each pixel occupies
; 4 bytes of CPU address space, so dest advances by 4 per pixel.
;
; Reads 4 source bytes at a time (word read from SDRAM) and writes
; them individually to consecutive VRAM word addresses.  This reduces
; source memory reads by 4x compared to per-byte readbu.
;
; Register usage (all caller-saved / argument regs):
;   r4 = src pointer (argument, incremented)
;   r1 = dest pointer (VRAMPX base, incremented)
;   r2 = end pointer (src + 64000)
;   r3 = packed 4 pixels (scratch)

.global doom_draw_frame_asm
doom_draw_frame_asm:
    load32 0x1EC00000 r1       ; dest = VRAMPX base
    load32 64000 r2            ; pixel count (320 * 200)
    add r4 r2 r2              ; end = src + 64000

.Ldoom_fb_loop:
    read 0 r4 r3              ; r3 = 4 packed pixels (LE: p0 in bits 7:0)

    writeb 0 r1 r3            ; write pixel 0 (bits 7:0)
    shiftr r3 8 r3
    add r1 4 r1
    writeb 0 r1 r3            ; write pixel 1 (bits 15:8)
    shiftr r3 8 r3
    add r1 4 r1
    writeb 0 r1 r3            ; write pixel 2 (bits 23:16)
    shiftr r3 8 r3
    add r1 4 r1
    writeb 0 r1 r3            ; write pixel 3 (bits 31:24)
    add r1 4 r1

    add r4 4 r4              ; src += 4
    bne r4 r2 .Ldoom_fb_loop  ; loop until all 64000 pixels copied

    jumpr 0 r15               ; return
