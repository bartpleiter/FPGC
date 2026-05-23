; fixed64_asm.asm — FP64 coprocessor primitive operations
;
; These functions provide C-callable wrappers for the FP64 coprocessor
; instructions. They always use fp6 and fp7 as temporary registers.
;
; The FP64 register index is encoded in the register field of the instruction
; (same encoding as CPU registers r0-r15, but the instruction targets the
; FP64 register file instead). For library use, fp6 and fp7 are dedicated
; temporaries. User code may use fp0-fp5 via these same helpers.

.text

; void fp64_hw_load6(int hi, int lo) — load hi/lo into fp6
; r4 = hi, r5 = lo
.global fp64_hw_load6
fp64_hw_load6:
    fld r4 r5 r6
    jumpr 0 r15

; void fp64_hw_load7(int hi, int lo) — load hi/lo into fp7
; r4 = hi, r5 = lo
.global fp64_hw_load7
fp64_hw_load7:
    fld r4 r5 r7
    jumpr 0 r15

; int fp64_hw_store_hi6() — store hi word of fp6 into r1
.global fp64_hw_store_hi6
fp64_hw_store_hi6:
    fsthi r6 r0 r1
    jumpr 0 r15

; int fp64_hw_store_lo6() — store lo word of fp6 into r1
.global fp64_hw_store_lo6
fp64_hw_store_lo6:
    fstlo r6 r0 r1
    jumpr 0 r15

; void fp64_hw_add66_7() — fp6 = fp6 + fp7
.global fp64_hw_add66_7
fp64_hw_add66_7:
    fadd r6 r7 r6
    jumpr 0 r15

; void fp64_hw_sub66_7() — fp6 = fp6 - fp7
.global fp64_hw_sub66_7
fp64_hw_sub66_7:
    fsub r6 r7 r6
    jumpr 0 r15

; void fp64_hw_mul66_7() — fp6 = fp6 * fp7
.global fp64_hw_mul66_7
fp64_hw_mul66_7:
    fmul r6 r7 r6
    jumpr 0 r15
