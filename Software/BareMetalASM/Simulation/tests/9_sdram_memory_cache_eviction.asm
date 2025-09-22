; Tests l1d cache eviction with dirty line write-back, assuming cache lines are 8 words
; This test verifies that when a dirty cache line is evicted, it is properly written back to SDRAM
; and can be correctly read back when the line is reloaded into cache
Main:

    ; Setup test data
    load 42 r1          ; Test data for address 0
    load 99 r2          ; Test data for address 1024  
    load 0 r3           ; Base address 0 (cache index 0)
    load32 1024 r4      ; Address 1024 (same cache index 0, different tag)
    
    ; Step 1: Write to address 0, creating a dirty cache line
    write 0 r3 r1       ; Write 42 to address 0 (cache index 0)
    
    ; Step 2: Write to address 1024, forcing eviction of dirty line from address 0
    write 0 r4 r2       ; Write 99 to address 1024 (same cache index 0, different tag)
    
    ; Step 3: Read from address 0 again
    read 0 r3 r5        ; Read from address 0 into r5, should be 42
    
    ; Read from address 1024 to verify it was also written correctly
    read 0 r4 r6        ; Read from address 1024 into r6, should be 99
    
    ; Add a nop here as we are not testing for the hazard where both alu inputs are from a multi-cycle read
    ; See test double_multicycle_hazard for that
    nop

    add r5 r6 r15       ; expected=141
    
    halt
