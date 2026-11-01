; The micro-benchmarks of CCACHE: one with four dirty lines, in lines 800 to 803
; (0x10000 to 0x10060), and one with none. A ccache retires once the walk is done. The
; ccached in between makes sure that the second ccache doesn't wait for its own line
; first: the first ccache redirects to it, at the start of line 1, which misses, and
; the controller only takes the ccached once that line is filled. CCACHED doesn't
; invalidate the L1I, so the second ccache hits. Line 1 is read three times: IF
; fetches it ahead behind the first ccache, and each ccache invalidates the L1I.
; runs from SDRAM
; sdram reads: 0 800 801 802 803 1 1 1
; sdram writes: 800 801 802 803
    load32 0x10000 r1
    write 0 r1 r1           ; four dirty lines
    write 0x20 r1 r1
    write 0x40 r1 r1
    write 0x60 r1 r1
    nop
    ccache                  ; word 7: writes back four lines
    ccached                 ; word 8, line 1
    ccache                  ; no dirty line
    halt
