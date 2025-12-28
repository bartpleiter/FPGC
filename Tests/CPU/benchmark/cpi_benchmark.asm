; CPI Benchmark - Tests different instruction patterns
; This benchmark measures cycles per instruction (CPI) for various workloads
;
; Test cases:
; 1. Simple ALU chain (no hazards)
; 2. Load-use hazard (read followed by use)
; 3. Multiply chain (multi-cycle ALU)
; 4. Branch-heavy code
; 5. Mixed workload
;
; The test counts cycles using r14 and instructions completed in r15

Main:
    ; Initialize counters
    load 0 r14              ; r14 = cycle counter (incremented each loop)
    load 0 r15              ; r15 = result accumulator
    
    ; =========================================================================
    ; TEST 1: Simple ALU chain (should be ~1 CPI in perfect pipeline)
    ; =========================================================================
    load 1 r1
    load 2 r2
    load 3 r3
    load 4 r4
    
    ; Chain of independent operations (no hazards)
    add r1 r2 r5            ; r5 = 3
    add r3 r4 r6            ; r6 = 7
    or r1 r3 r7             ; r7 = 3
    and r2 r4 r8            ; r8 = 0
    xor r1 r4 r9            ; r9 = 5
    sub r6 r5 r10           ; r10 = 4
    
    add r15 r5 r15          ; r15 += 3
    add r15 r6 r15          ; r15 += 7 (=10)
    add r15 r10 r15         ; r15 += 4 (=14)
    
    ; =========================================================================
    ; TEST 2: Load-use hazard pattern (causes stalls)
    ; =========================================================================
    write 0 r0 r1           ; Store 1 to addr 0
    write 4 r0 r2           ; Store 2 to addr 4
    
    read 0 r0 r11           ; Load from addr 0
    add r11 r1 r11          ; Use immediately (load-use hazard)
    
    read 4 r0 r12           ; Load from addr 4  
    add r12 r2 r12          ; Use immediately (load-use hazard)
    
    add r15 r11 r15         ; r15 += 2 (=16)
    add r15 r12 r15         ; r15 += 4 (=20)
    
    ; =========================================================================
    ; TEST 3: Multiply chain (multi-cycle operations)
    ; =========================================================================
    load 5 r1
    load 6 r2
    
    multu r1 r2 r11         ; r11 = 30
    multu r11 r1 r12        ; r12 = 150 (depends on r11 - multi-cycle hazard)
    
    ; Use unsigned division  
    load 100 r3
    load 10 r4
    divu r3 r4 r13          ; r13 = 10 (100/10)
    
    add r15 r13 r15         ; r15 += 10 (=30)
    
    ; =========================================================================
    ; TEST 4: Branch-heavy code (control hazards)
    ; =========================================================================
    load 5 r1               ; Loop counter
    load 0 r2               ; Sum
    
BranchLoop:
    add r2 r1 r2            ; sum += counter
    sub r1 1 r1             ; counter--
    bne r0 r1 BranchLoop    ; if counter != 0, loop
    
    add r15 r2 r15          ; r15 += 15 (=45, since 5+4+3+2+1=15)
    
    ; =========================================================================
    ; TEST 5: Mixed workload with data dependencies
    ; =========================================================================
    load 7 r1
    load 8 r2
    
    add r1 r2 r3            ; r3 = 15
    multu r3 r1 r4          ; r4 = 105 (depends on r3)
    sub r4 r3 r5            ; r5 = 90 (depends on r4)
    
    ; Store and reload (cache interaction)
    write 8 r0 r5           ; Store 90 to addr 8
    read 8 r0 r6            ; Load back (cache hit)
    
    ; Final calculation
    load 45 r7
    add r6 r7 r15           ; r15 = 90 + 45 = 135
    
    ; Done!
    halt

; Expected final result: r15 = 135
