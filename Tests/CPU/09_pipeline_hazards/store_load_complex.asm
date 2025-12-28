; Test store-to-load forwarding with multiple cycles between
; This tests if stalling until write completes works correctly
Main:
    load32 0x77FFFF r14   ; init base pointer to stack area
    
    ; Test 1: Basic write then read
    load 42 r1
    write -1 r14 r1       ; write 42 to 0x77FFFE
    read -1 r14 r2        ; read from same address
    
    ; Check result
    xor r2 42 r3
    beq r0 r3 test2
    load 1 r15            ; FAIL: test 1
    halt
    
test2:
    ; Test 2: Write then other instruction then read
    load 100 r1
    write -2 r14 r1       ; write 100 to 0x77FFFD
    add r0 r0 r0          ; NOP
    read -2 r14 r2        ; read from same address
    
    xor r2 100 r3
    beq r0 r3 test3
    load 2 r15            ; FAIL: test 2
    halt
    
test3:
    ; All passed
    load 42 r15           ; expected=42
    halt
