; Test: VRAM32 clear loop (bootloader pattern)
; Exactly mimics the bootloader ClearVRAM32 loop pattern
; Uses `write 0 r1 r0` which writes r0 (always 0) to address r1+0
; Then reads back and verifies the data is 0

Main:
    ; First, write non-zero values to VRAM32 to ensure it's not already 0
    load32 0x7900000 r1     ; VRAM32 address
    load 0 r2               ; counter
    load 5 r3               ; loop end

    WriteNonZeroLoop:
        add r2 1 r4          ; r4 = counter + 1 (non-zero value)
        write 0 r1 r4        ; write non-zero to VRAM
        add r1 1 r1          ; increase address
        add r2 1 r2          ; increase counter
        beq r3 r2 2          ; if done, skip jump
        jump WriteNonZeroLoop

    ; Now clear VRAM32 exactly like bootloader does
    load32 0x7900000 r1     ; VRAM32 address
    load 0 r2               ; r2 = 0, counter
    load 5 r3               ; r3 = loop end (smaller for test)

    ClearVRAM32Loop:
        write 0 r1 r0        ; write 0 to VRAM (r0 is always 0)
        add r1 1 r1          ; increase address
        add r2 1 r2          ; increase counter
        beq r3 r2 2          ; keep looping until all words are cleared
        jump ClearVRAM32Loop

    ; Read back and verify all values are 0
    load32 0x7900000 r1     ; VRAM32 base address
    load 0 r5               ; sum should stay 0 if all values are 0
    
    read 0 r1 r6            ; read addr 0
    add r5 r6 r5            ; sum += value (should be 0)
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 1
    add r5 r6 r5            ; sum += value (should be 0)
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 2
    add r5 r6 r5            ; sum += value (should be 0)
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 3
    add r5 r6 r5            ; sum += value (should be 0)
    add r1 1 r1
    
    read 0 r1 r6            ; read addr 4
    add r5 r6 r5            ; sum += value (should be 0)
    
    ; If all values were properly cleared, sum = 0
    ; Return 42 if sum is 0 (success), otherwise return sum (shows corruption)
    bne r5 r0 2
    load 42 r5              ; Success!
    
    or r0 r5 r15            ; expected=42

    halt
