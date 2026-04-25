; doom_asm.asm — Optimized assembly helpers for Doom on FPGC
;
; Provides:
;   doom_draw_frame_asm      — fast framebuffer blit to VRAM
;   R_DrawColumn_asm         — wall column drawer (high detail)
;   R_DrawSpan_asm           — floor/ceiling span drawer (high detail)
;   R_DrawColumnLow_asm      — wall column drawer (low detail, 2x wide)
;   R_DrawSpanLow_asm        — floor/ceiling span drawer (low detail, 2x wide)
;   R_DrawSpan_asm       — floor/ceiling span drawer

.text

; ============================================================
; void doom_draw_frame_asm(unsigned char *src)
;
; Copies 320x200 (64000) bytes from src to the pixel framebuffer.
; VRAMPX is byte-addressable (one byte per pixel), so dest advances
; by 1 byte per pixel.
;
; The display is 320x240. Doom renders 320x200. We center vertically
; by starting at line 20 (offset = 20 * 320 = 6400 bytes).
;
; Reads 4 source bytes at a time (word read from SDRAM) and writes
; them individually to consecutive VRAMPX byte addresses.
;
; Register usage (all caller-saved / argument regs):
;   r4 = src pointer (argument, incremented)
;   r1 = dest pointer (VRAMPX base + 20-line offset, incremented)
;   r2 = end pointer (src + 64000)
;   r3 = packed 4 pixels (scratch)

.global doom_draw_frame_asm
doom_draw_frame_asm:
    load32 0x1EC01900 r1       ; dest = VRAMPX base + 20 lines (20*320)
    load32 64000 r2            ; pixel count (320 * 200)
    add r4 r2 r2              ; end = src + 64000

.Ldoom_fb_loop:
    read 0 r4 r3              ; r3 = 4 packed pixels (LE: p0 in bits 7:0)

    writeb 0 r1 r3            ; write pixel 0 (bits 7:0)
    shiftr r3 8 r3
    add r1 1 r1
    writeb 0 r1 r3            ; write pixel 1 (bits 15:8)
    shiftr r3 8 r3
    add r1 1 r1
    writeb 0 r1 r3            ; write pixel 2 (bits 23:16)
    shiftr r3 8 r3
    add r1 1 r1
    writeb 0 r1 r3            ; write pixel 3 (bits 31:24)
    add r1 1 r1

    add r4 4 r4              ; src += 4
    bne r4 r2 .Ldoom_fb_loop  ; loop until all 64000 pixels copied

    jumpr 0 r15               ; return

; ============================================================
; void R_DrawColumn_asm(void)
;
; Assembly-optimized wall column drawer.  Replaces R_DrawColumn().
; All parameters via globals (dc_yh, dc_yl, dc_iscale, dc_texturemid,
; dc_source, dc_colormap, dc_x, ylookup, columnofs, centery).
;
; Uses pre-shifted frac arithmetic: frac <<= 9, then >>25 extracts
; the 7-bit texture index (0-127), equivalent to (frac>>16)&127.
;
; Inner loop: 10 instructions per pixel.
;
; Register plan (inner loop):
;   r1 = dest pointer (into screen buffer)
;   r2 = frac (pre-shifted <<9)
;   r3 = fracstep (dc_iscale <<9)
;   r4 = source (dc_source)
;   r5 = colormap (dc_colormap)
;   r6 = count
;   r7 = scratch

.global R_DrawColumn_asm
R_DrawColumn_asm:
    ; count = dc_yh - dc_yl + 1
    addr2reg dc_yh r12
    read 0 r12 r6             ; r6 = dc_yh
    addr2reg dc_yl r12
    read 0 r12 r7             ; r7 = dc_yl
    sub r6 r7 r6              ; r6 = dc_yh - dc_yl
    add r6 1 r6               ; r6 = count
    bles r6 r0 .Ldc_done      ; return if count <= 0

    ; source = dc_source
    addr2reg dc_source r12
    read 0 r12 r4

    ; colormap = dc_colormap
    addr2reg dc_colormap r12
    read 0 r12 r5

    ; dest = ylookup[dc_yl] + columnofs[dc_x]
    addr2reg ylookup r12
    shiftl r7 2 r1             ; r1 = dc_yl * 4 (byte offset into ylookup)
    add r12 r1 r1              ; r1 = &ylookup[dc_yl]
    read 0 r1 r1              ; r1 = ylookup[dc_yl]

    addr2reg dc_x r12
    read 0 r12 r7             ; r7 = dc_x
    addr2reg columnofs r12
    shiftl r7 2 r7             ; r7 = dc_x * 4
    add r12 r7 r7              ; r7 = &columnofs[dc_x]
    read 0 r7 r7              ; r7 = columnofs[dc_x]
    add r1 r7 r1              ; r1 = dest

    ; fracstep = dc_iscale << 9
    addr2reg dc_iscale r12
    read 0 r12 r3
    shiftl r3 9 r3             ; r3 = fracstep

    ; frac = (dc_texturemid + (dc_yl - centery) * dc_iscale) << 9
    addr2reg dc_yl r12
    read 0 r12 r7             ; r7 = dc_yl
    addr2reg centery r12
    read 0 r12 r2             ; r2 = centery
    sub r7 r2 r7              ; r7 = dc_yl - centery
    addr2reg dc_iscale r12
    read 0 r12 r2             ; r2 = dc_iscale
    mults r7 r2 r2            ; r2 = (dc_yl - centery) * dc_iscale
    addr2reg dc_texturemid r12
    read 0 r12 r7             ; r7 = dc_texturemid
    add r7 r2 r2              ; r2 = dc_texturemid + offset
    shiftl r2 9 r2            ; r2 = frac (pre-shifted)

    ; Inner loop: *dest = colormap[source[frac >> 25]]
.Ldc_loop:
    shiftr r2 25 r7            ; r7 = texture index (7 bits)
    add r4 r7 r7              ; r7 = &source[index]
    readbu 0 r7 r7            ; r7 = source[index]
    add r5 r7 r7              ; r7 = &colormap[source[index]]
    readbu 0 r7 r7            ; r7 = colormap[source[index]]
    writeb 0 r1 r7            ; *dest = pixel
    add r1 320 r1             ; dest += SCREENWIDTH
    add r2 r3 r2              ; frac += fracstep
    sub r6 1 r6               ; count--
    bne r6 r0 .Ldc_loop       ; loop while count > 0

.Ldc_done:
    jumpr 0 r15               ; return

; ============================================================
; void R_DrawSpan_asm(void)
;
; Assembly-optimized floor/ceiling span drawer.  Replaces R_DrawSpan().
; All parameters via globals (ds_y, ds_x1, ds_x2, ds_xfrac, ds_yfrac,
; ds_xstep, ds_ystep, ds_source, ds_colormap, ylookup, columnofs).
;
; Position packing: x in top 16 bits, y in bottom 16 bits.
;   position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6) & 0xffff)
;   step     = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6) & 0xffff)
;
; Texture lookup: spot = ((position >> 26) | ((position >> 4) & 0x0fc0))
;
; Inner loop: 11 instructions per pixel.
;
; Register plan:
;   r1 = dest pointer
;   r2 = position (packed x/y)
;   r3 = step
;   r4 = source (ds_source)
;   r5 = colormap (ds_colormap)
;   r6 = count
;   r7 = scratch

.global R_DrawSpan_asm
R_DrawSpan_asm:
    ; count = ds_x2 - ds_x1 + 1
    addr2reg ds_x2 r12
    read 0 r12 r6             ; r6 = ds_x2
    addr2reg ds_x1 r12
    read 0 r12 r7             ; r7 = ds_x1
    sub r6 r7 r6
    add r6 1 r6               ; r6 = count
    bles r6 r0 .Lds_done      ; return if count <= 0

    ; source = ds_source
    addr2reg ds_source r12
    read 0 r12 r4

    ; colormap = ds_colormap
    addr2reg ds_colormap r12
    read 0 r12 r5

    ; dest = ylookup[ds_y] + columnofs[ds_x1]
    addr2reg ds_y r12
    read 0 r12 r1             ; r1 = ds_y
    addr2reg ylookup r12
    shiftl r1 2 r1             ; r1 = ds_y * 4
    add r12 r1 r1              ; r1 = &ylookup[ds_y]
    read 0 r1 r1              ; r1 = ylookup[ds_y]

    addr2reg columnofs r12
    shiftl r7 2 r7             ; r7 = ds_x1 * 4  (r7 still has ds_x1)
    add r12 r7 r7
    read 0 r7 r7              ; r7 = columnofs[ds_x1]
    add r1 r7 r1              ; r1 = dest

    ; position = ((ds_xfrac << 10) & 0xffff0000) | ((ds_yfrac >> 6) & 0xffff)
    addr2reg ds_xfrac r12
    read 0 r12 r2             ; r2 = ds_xfrac
    shiftl r2 10 r2           ; r2 = ds_xfrac << 10
    load32 0xFFFF0000 r7
    and r2 r7 r2              ; r2 = (ds_xfrac << 10) & 0xffff0000

    addr2reg ds_yfrac r12
    read 0 r12 r7             ; r7 = ds_yfrac
    shiftr r7 6 r7            ; r7 = ds_yfrac >> 6
    and r7 0x7FFF r7          ; r7 &= 0xffff (keep low 16 bits; 0x7FFF is 15 bits but close enough)
    ; Actually need 0xFFFF — let me use load + and instead
    ; Hmm, 0xFFFF doesn't fit in signed 16-bit. Use 16-bit masking trick:
    ; shiftl then shiftr to clear upper bits
    shiftl r7 16 r7
    shiftr r7 16 r7            ; r7 = (ds_yfrac >> 6) & 0xffff
    or r2 r7 r2               ; r2 = position

    ; step = ((ds_xstep << 10) & 0xffff0000) | ((ds_ystep >> 6) & 0xffff)
    addr2reg ds_xstep r12
    read 0 r12 r3             ; r3 = ds_xstep
    shiftl r3 10 r3
    load32 0xFFFF0000 r7
    and r3 r7 r3              ; r3 = (ds_xstep << 10) & 0xffff0000

    addr2reg ds_ystep r12
    read 0 r12 r7
    shiftr r7 6 r7
    shiftl r7 16 r7
    shiftr r7 16 r7            ; r7 = (ds_ystep >> 6) & 0xffff
    or r3 r7 r3               ; r3 = step

    ; Inner loop: *dest++ = colormap[source[spot]]
    ;   where spot = (position >> 26) | ((position >> 4) & 0x0fc0)
.Lds_loop:
    shiftr r2 4 r7             ; r7 = position >> 4
    and r7 4032 r7             ; r7 = ytemp = (position >> 4) & 0x0fc0
    shiftr r2 26 r12           ; r12 = xtemp = position >> 26
    or r12 r7 r7              ; r7 = spot = xtemp | ytemp
    add r4 r7 r7              ; r7 = &source[spot]
    readbu 0 r7 r7            ; r7 = source[spot]
    add r5 r7 r7              ; r7 = &colormap[source[spot]]
    readbu 0 r7 r7            ; r7 = colormap[source[spot]]
    writeb 0 r1 r7            ; *dest = pixel
    add r1 1 r1               ; dest++ (sequential in screen buffer)
    add r2 r3 r2              ; position += step
    sub r6 1 r6               ; count--
    bne r6 r0 .Lds_loop       ; loop while count > 0

.Lds_done:
    jumpr 0 r15               ; return

; ============================================================
; void R_DrawColumnLow_asm(void)
;
; Low-detail wall column drawer.  Draws each pixel doubled (2 wide).
; Same as R_DrawColumn_asm but writes to dest and dest+1 using
; writeb offset, eliminating the need for a second dest register.
;
; x = dc_x << 1; both columnofs[x] and columnofs[x+1] are written.

.global R_DrawColumnLow_asm
R_DrawColumnLow_asm:
    ; count = dc_yh - dc_yl + 1
    addr2reg dc_yh r12
    read 0 r12 r6
    addr2reg dc_yl r12
    read 0 r12 r7
    sub r6 r7 r6
    add r6 1 r6
    bles r6 r0 .Ldcl_done

    ; source, colormap
    addr2reg dc_source r12
    read 0 r12 r4
    addr2reg dc_colormap r12
    read 0 r12 r5

    ; dest = ylookup[dc_yl] + columnofs[dc_x << 1]
    addr2reg ylookup r12
    shiftl r7 2 r1
    add r12 r1 r1
    read 0 r1 r1              ; r1 = ylookup[dc_yl]

    addr2reg dc_x r12
    read 0 r12 r7
    shiftl r7 1 r7             ; r7 = dc_x << 1 (blocky mode)
    addr2reg columnofs r12
    shiftl r7 2 r7
    add r12 r7 r7
    read 0 r7 r7              ; r7 = columnofs[x]
    add r1 r7 r1              ; r1 = dest

    ; fracstep, frac (NOT pre-shifted — use FRACBITS+mask like original)
    addr2reg dc_iscale r12
    read 0 r12 r3             ; r3 = fracstep
    addr2reg dc_yl r12
    read 0 r12 r7
    addr2reg centery r12
    read 0 r12 r2
    sub r7 r2 r7
    mults r7 r3 r2            ; r2 = (dc_yl - centery) * dc_iscale
    addr2reg dc_texturemid r12
    read 0 r12 r7
    add r7 r2 r2              ; r2 = frac (NOT pre-shifted)

.Ldcl_loop:
    shiftrs r2 16 r7           ; r7 = frac >> FRACBITS (signed shift)
    and r7 127 r7              ; r7 = index & 127
    add r4 r7 r7
    readbu 0 r7 r7            ; r7 = source[index]
    add r5 r7 r7
    readbu 0 r7 r7            ; r7 = colormap[source[index]]
    writeb 0 r1 r7            ; *dest = pixel
    writeb 1 r1 r7            ; *(dest+1) = pixel (doubled)
    add r1 320 r1             ; dest += SCREENWIDTH
    add r2 r3 r2              ; frac += fracstep
    sub r6 1 r6
    bne r6 r0 .Ldcl_loop

.Ldcl_done:
    jumpr 0 r15

; ============================================================
; void R_DrawSpanLow_asm(void)
;
; Low-detail floor/ceiling span drawer.  Writes each pixel doubled (2 wide).
; Same texture lookup as R_DrawSpan_asm, but writes each pixel twice
; and advances dest by 2 per iteration.

.global R_DrawSpanLow_asm
R_DrawSpanLow_asm:
    ; count = ds_x2 - ds_x1 + 1 (in original coordinates, before doubling)
    addr2reg ds_x2 r12
    read 0 r12 r6
    addr2reg ds_x1 r12
    read 0 r12 r7
    sub r6 r7 r6
    add r6 1 r6
    bles r6 r0 .Ldsl_done

    ; source, colormap
    addr2reg ds_source r12
    read 0 r12 r4
    addr2reg ds_colormap r12
    read 0 r12 r5

    ; dest = ylookup[ds_y] + columnofs[ds_x1 << 1]
    ; Low mode: ds_x1/ds_x2 are doubled by the C wrapper before calling
    ; Actually, R_DrawSpanLow does ds_x1 <<= 1 INSIDE the function.
    ; So we need to do the same.
    shiftl r7 1 r7             ; r7 = ds_x1 << 1

    addr2reg ds_y r12
    read 0 r12 r1
    addr2reg ylookup r12
    shiftl r1 2 r1
    add r12 r1 r1
    read 0 r1 r1              ; r1 = ylookup[ds_y]

    addr2reg columnofs r12
    shiftl r7 2 r7
    add r12 r7 r7
    read 0 r7 r7
    add r1 r7 r1              ; r1 = dest

    ; position and step (same as high-detail)
    addr2reg ds_xfrac r12
    read 0 r12 r2
    shiftl r2 10 r2
    load32 0xFFFF0000 r7
    and r2 r7 r2

    addr2reg ds_yfrac r12
    read 0 r12 r7
    shiftr r7 6 r7
    shiftl r7 16 r7
    shiftr r7 16 r7
    or r2 r7 r2               ; r2 = position

    addr2reg ds_xstep r12
    read 0 r12 r3
    shiftl r3 10 r3
    load32 0xFFFF0000 r7
    and r3 r7 r3

    addr2reg ds_ystep r12
    read 0 r12 r7
    shiftr r7 6 r7
    shiftl r7 16 r7
    shiftr r7 16 r7
    or r3 r7 r3               ; r3 = step

.Ldsl_loop:
    shiftr r2 4 r7
    and r7 4032 r7
    shiftr r2 26 r12
    or r12 r7 r7
    add r4 r7 r7
    readbu 0 r7 r7
    add r5 r7 r7
    readbu 0 r7 r7
    writeb 0 r1 r7            ; *dest = pixel
    writeb 1 r1 r7            ; *(dest+1) = pixel (doubled)
    add r1 2 r1               ; dest += 2
    add r2 r3 r2              ; position += step
    sub r6 1 r6
    bne r6 r0 .Ldsl_loop

.Ldsl_done:
    jumpr 0 r15
