; Tests that the new ccached instruction (data-cache-only flush+invalidate)
; correctly evicts dirty L1d lines back to SDRAM.
;
; If ccached works:
;   - write to address N caches a dirty line in L1d
;   - ccached flushes dirty lines to SDRAM and invalidates L1d
;   - subsequent read misses L1d, fetches from SDRAM, returns the written value
;
; If ccached's L1d flush is broken (e.g. it only invalidates without writeback):
;   - the read would still pick up the value because the L1d line is just gone
;     and SDRAM was already updated... wait, in a write-back L1d, SDRAM is NOT
;     updated until eviction. So if ccached doesn't flush, the dirty line is
;     lost on invalidate and SDRAM still holds the old value.
;
; To make the failure mode observable we initialise the SDRAM location to a
; sentinel via a write+evict pattern first, then overwrite via L1d, ccached,
; and read back. The expected r15 value is the second write value.
Main:
    ; Constants
    load 42  r1         ; sentinel value (initial)
    load 99  r2         ; "after-flush" value (what we expect to read back)
    load 0   r3         ; address A
    load32 4096 r4      ; address B (same cache index, different tag -> forces eviction)

    ; --- Step 1: prime SDRAM at address A with the sentinel ---
    write 0 r3 r1       ; write 42 to A (dirty in L1d)
    write 0 r4 r1       ; write 42 to B -> evicts A's dirty line to SDRAM
    read  0 r3 r5       ; reload A into L1d (clean, value=42)

    ; --- Step 2: overwrite A in L1d with 99, mark dirty, do NOT touch B again ---
    write 0 r3 r2       ; write 99 to A (dirty L1d line, SDRAM still 42)

    ; --- Step 3: ccached should flush 99 back to SDRAM and invalidate L1d ---
    ccached

    ; --- Step 4: read A. Cache miss (L1d invalidated) -> fetch from SDRAM. ---
    ; If ccached flushed correctly, SDRAM has 99 -> r6 == 99.
    ; If ccached only invalidated, SDRAM has 42 -> r6 == 42 (test fails).
    read 0 r3 r6

    ; --- Step 5: write the result to r15 for the test runner ---
    nop
    or r6 0 r15         ; expected=99

    halt
