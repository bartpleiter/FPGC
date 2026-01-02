; Test: VRAM loop write
; Simulates the bootloader VRAM clear loop pattern
; Writes values to consecutive VRAM32 addresses and verifies

Main:
    ; Write values 1-5 to VRAM32 addresses 0-4
    load32 0x7900000 r1     ; VRAM32 base address
    load 1 r2               ; value to write
    load 5 r3               ; loop count
    load 0 r4               ; counter
    
WriteLoop:
    write 0 r1 r2           ; write value to VRAM
    add r1 1 r1             ; increase address
    add r2 1 r2             ; increase value
    add r4 1 r4             ; increase counter
    beq r3 r4 2             ; if counter == 5, skip jump
    jump WriteLoop

    ; Read back and verify
    load32 0x7900000 r1     ; VRAM32 base address
    load 0 r5               ; sum = 0
    
    read 0 r1 r6            ; read addr 0, should be 1
    add r5 r6 r5            ; sum += value
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 1, should be 2  
    add r5 r6 r5            ; sum += value
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 2, should be 3
    add r5 r6 r5            ; sum += value
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 3, should be 4
    add r5 r6 r5            ; sum += value
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 4, should be 5
    add r5 r6 r5            ; sum += value
    
    ; sum should be 1+2+3+4+5 = 15
    or r0 r5 r15            ; expected=15

    halt
