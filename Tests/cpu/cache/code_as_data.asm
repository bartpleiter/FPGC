; Code written as data, then CCACHE, then executed: the case that rule 2.27 allows.
; Target first runs as it is (r5 = 1), so the L1I holds its line. The program then
; copies the two words of Template over it, through the L1D, so the new code is only in
; a dirty line of the L1D. CCACHE writes that line back and invalidates the L1I, so the
; second jump to Target fetches the new code from SDRAM (r5 = 2). Target returns to the
; address in r7.
; runs from SDRAM
; sdram reads: 0 2 1 3 2 1 2
; sdram writes: 2
    addr2reg Back1 r7
    jump Target             ; runs the old code: r5 = 1
Back1:
    addr2reg Template r1
    addr2reg Target r2
    read 0 r1 r3
    write 0 r2 r3
    read 4 r1 r3
    write 4 r2 r3
    addr2reg Back2 r7
    ccache                  ; writes line 2 back, and invalidates both caches
    jump Target             ; runs the new code: r5 = 2
Back2:
    halt
Target:                     ; line 2 (0x40)
    load 1 r5
    jumpr 0 r7
    nop
    nop
    nop
    nop
    nop
    nop
Template:                   ; line 3 (0x60)
    load 2 r5
    jumpr 0 r7
