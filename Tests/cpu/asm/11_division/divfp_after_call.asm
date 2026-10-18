; Test: divfp at function return target
; Test 1: divfp right after call return (the problematic pattern)
; Expected: divfp(6<<16, 2<<16) = 3.0 in Q16.16 = 196608, >> 16 = 3
; Return 3 + 4 = 7

Main:
    ; Setup: r13 = SP
    load32 0x1DFFFFC r13

    ; Call shift16(6) — returns 6<<16 = 393216 in r1
    load 6 r4
    savpc r15
    add r15 12 r15
    jump shift16
    ; Return here with r1 = 393216
    or r0 r1 r8        ; r8 = 393216

    ; Call shift16(2) — returns 2<<16 = 131072 in r1
    load 2 r4
    savpc r15
    add r15 12 r15
    jump shift16
    ; Return here with r1 = 131072
    ; NOTE: divfp is the first instruction at the return target  
    divfp r8 r1 r2     ; r2 = divfp(393216, 131072) should be 196608
    shiftrs r2 16 r15  ; r15 = 3, expected=3

    halt

; Subroutine: shift16(r4) → r1 = r4 << 16
shift16:
    write 0 r13 r14
    write 4 r13 r15
    or r0 r13 r14
    sub r13 8 r13
    shiftl r4 16 r1
    or r0 r14 r13
    read 4 r14 r15
    read 0 r14 r14
    jumpr 0 r15
