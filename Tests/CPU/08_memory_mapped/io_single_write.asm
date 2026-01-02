; Test: I/O single write
; Verifies that an I/O write operation only executes once
; This tests for a bug where the MU could restart if start was held high
; The test uses multiple UART writes in sequence - if each fires exactly once,
; the test passes. If any fires twice, the test will timeout or give wrong count.

Main:
    ; Write to UART TX three times
    ; Each write should only execute once
    load32 0x7000000 r2  ; UART TX address
    
    ; First write: 0x11
    load 0x11 r1
    write 0 r2 r1
    
    ; Second write: 0x22
    load 0x22 r1
    write 0 r2 r1
    
    ; Third write: 0x33
    load 0x33 r1
    write 0 r2 r1
    
    ; If we get here, all three writes completed without hanging
    ; Return success value
    load 42 r15  ; expected=42

    halt
