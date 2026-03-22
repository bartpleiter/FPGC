; mbrotc_asm.asm — Mandelbrot iteration kernel for the cluster worker.
;
; FP64 coprocessor register allocation:
;   f0 = c_re (complex coordinate real)
;   f1 = c_im (complex coordinate imaginary)
;   f2 = z_re (iteration state)
;   f3 = z_im (iteration state)
;   f4 = scratch (z_re^2 / |z|^2)
;   f5 = scratch (z_im^2)
;   f6 = scratch (z_re * z_im)
;   f7 = step (external use by C code)
;
; void mbrotc_load_cre(int hi, int lo) — load c_re into f0
; void mbrotc_load_cim(int hi, int lo) — load c_im into f1
; void mbrotc_load_step(int hi, int lo) — load step into f7
; void mbrotc_advance_cre() — f0 += f7 (advance c_re by step)
; void mbrotc_advance_cim() — f1 += f7 (advance c_im by step)
; int  mbrotc_store_hi_cre() — store hi word of f0
; int  mbrotc_store_lo_cre() — store lo word of f0
; int  mbrotc_store_hi_step() — store hi word of f7
; int  mbrotc_store_lo_step() — store lo word of f7
;
; FP64 scratch operations for view setup:
; void mbrotc_load_f6(int hi, int lo)
; void mbrotc_load_f7(int hi, int lo)
; void mbrotc_mul_f7_f6() — f7 = f7 * f6
; void mbrotc_sub_f0_f6() — f0 = f0 - f6
; void mbrotc_sub_f1_f6() — f1 = f1 - f6
; int  mbrotc_store_hi_f7()
; int  mbrotc_store_lo_f7()
;
; int mbrotc_mandelbrot_pixel(int max_iter) — compute iteration count.
;   f0 and f1 must be loaded with c_re and c_im before calling.
;   Returns 0 if in set, 1..max_iter if escaped.

.text

.global mbrotc_load_cre
mbrotc_load_cre:
    fld r4 r5 r0
    jumpr 0 r15

.global mbrotc_load_cim
mbrotc_load_cim:
    fld r4 r5 r1
    jumpr 0 r15

.global mbrotc_load_step
mbrotc_load_step:
    fld r4 r5 r7
    jumpr 0 r15

.global mbrotc_advance_cre
mbrotc_advance_cre:
    fadd r0 r7 r0
    jumpr 0 r15

.global mbrotc_advance_cim
mbrotc_advance_cim:
    fadd r1 r7 r1
    jumpr 0 r15

.global mbrotc_store_hi_cre
mbrotc_store_hi_cre:
    fsthi r0 r0 r1
    jumpr 0 r15

.global mbrotc_store_lo_cre
mbrotc_store_lo_cre:
    fstlo r0 r0 r1
    jumpr 0 r15

.global mbrotc_store_hi_step
mbrotc_store_hi_step:
    fsthi r7 r0 r1
    jumpr 0 r15

.global mbrotc_store_lo_step
mbrotc_store_lo_step:
    fstlo r7 r0 r1
    jumpr 0 r15

.global mbrotc_load_f6
mbrotc_load_f6:
    fld r4 r5 r6
    jumpr 0 r15

.global mbrotc_load_f7
mbrotc_load_f7:
    fld r4 r5 r7
    jumpr 0 r15

.global mbrotc_mul_f7_f6
mbrotc_mul_f7_f6:
    fmul r7 r6 r7
    jumpr 0 r15

.global mbrotc_sub_f0_f6
mbrotc_sub_f0_f6:
    fsub r0 r6 r0
    jumpr 0 r15

.global mbrotc_sub_f1_f6
mbrotc_sub_f1_f6:
    fsub r1 r6 r1
    jumpr 0 r15

.global mbrotc_store_hi_f7
mbrotc_store_hi_f7:
    fsthi r7 r0 r1
    jumpr 0 r15

.global mbrotc_store_lo_f7
mbrotc_store_lo_f7:
    fstlo r7 r0 r1
    jumpr 0 r15

; int mbrotc_mandelbrot_pixel(int max_iter)
; r4 = max_iter. Returns iteration count in r1.
; FP regs: f0=c_re, f1=c_im, f2=z_re, f3=z_im, f4-f6=scratch
.global mbrotc_mandelbrot_pixel
mbrotc_mandelbrot_pixel:
    ; z_re = z_im = 0
    fld r0 r0 r2
    fld r0 r0 r3

    ; iter = 0
    or r0 r0 r5

    ; Edge case: max_iter == 0
    beq r4 r0 Label_mbrotc_set

Label_mbrotc_loop:
    fmul r2 r2 r4       ; f4 = z_re^2
    fmul r3 r3 r5       ; f5 = z_im^2
    fmul r2 r3 r6       ; f6 = z_re * z_im
    fsub r4 r5 r2       ; f2 = z_re^2 - z_im^2
    fadd r2 r0 r2       ; f2 += c_re  (new z_re)
    fadd r6 r6 r3       ; f3 = 2 * z_re * z_im
    fadd r3 r1 r3       ; f3 += c_im  (new z_im)

    ; Escape check: |z|^2 >= 4
    fadd r4 r5 r4       ; f4 = |z|^2
    fsthi r4 r0 r1      ; r1 = integer part of |z|^2
    sltu r1 4 r1        ; r1 = 1 if mag < 4
    beq r1 r0 Label_mbrotc_escaped

    add r5 1 r5
    slt r5 r4 r1        ; iter < max_iter?
    bne r1 r0 Label_mbrotc_loop

Label_mbrotc_set:
    or r0 r0 r1         ; return 0
    jumpr 0 r15

Label_mbrotc_escaped:
    add r5 1 r1         ; return iter + 1
    jumpr 0 r15
