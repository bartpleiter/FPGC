; Test: ROM to RAM copy (bootloader pattern)
; Simulates the bootloader copying code from ROM data section to RAM
; This pattern is used by CopyUARTbootloader and CopySPIprogramToRAM

Main:
    ; Set up source data in ROM (at label DataSection)
    ; We'll copy 5 words from ROM data to RAM addresses 0-4
    
    addr2reg DataSection r1  ; Source address (ROM)
    load 0 r2                ; Destination address (RAM)
    load 5 r3                ; Number of words to copy
    load 0 r4                ; Counter
    
CopyLoop:
    read 0 r1 r5             ; Read word from ROM
    write 0 r2 r5            ; Write word to RAM
    add r1 1 r1              ; Increment source
    add r2 1 r2              ; Increment destination
    add r4 1 r4              ; Increment counter
    beq r4 r3 2              ; If done, skip jump
    jump CopyLoop

    ; Now read back from RAM and sum the values
    load 0 r1                ; RAM start address
    load 0 r6                ; sum = 0
    load 0 r4                ; counter
    
ReadBackLoop:
    read 0 r1 r5             ; Read from RAM
    add r6 r5 r6             ; sum += value
    add r1 1 r1              ; Next address
    add r4 1 r4              ; Increment counter
    beq r4 r3 2              ; If done, skip jump
    jump ReadBackLoop
    
    ; Sum should be 1+2+3+4+5 = 15
    or r0 r6 r15             ; expected=15
    
    halt

DataSection:
    .dw 1   ; Value at DataSection+0
    .dw 2   ; Value at DataSection+1
    .dw 3   ; Value at DataSection+2
    .dw 4   ; Value at DataSection+3
    .dw 5   ; Value at DataSection+4
