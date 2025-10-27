; Debugging program for SDRAM testing (find optimal phase shift)
Main:

    load32 0x7000000 r10 ; MU address
    load 64 r11
    write 0 r10 r11      ; Write test byte over UART
        
    ; Setup test data
    load 48 r1          ; Test data for address 0
    load 70 r2          ; Test data for address 1024  
    load 0 r3           ; Base address 0 (cache index 0)
    load32 1024 r4      ; Address 1024 (same cache index 0, different tag)
    
    ; Step 1: Write to address 0 to 7, creating a full dirty cache line
    write 0 r3 r1       ; Write 48 to address 0 (cache index 0)
    add r1 1 r1
    write 1 r3 r1       ; Write 49 to address 1
    add r1 1 r1
    write 2 r3 r1       ; Write 50 to address 2
    add r1 1 r1
    write 3 r3 r1       ; Write 51 to address 3
    add r1 1 r1
    write 4 r3 r1       ; Write 52 to address 4
    add r1 1 r1
    write 5 r3 r1       ; Write 53 to address 5
    add r1 1 r1
    write 6 r3 r1       ; Write 54 to address 6
    add r1 1 r1
    write 7 r3 r1       ; Write 55 to address 7
    
    ; Step 2: Write to address 1024, forcing eviction of dirty line from address 0
    write 0 r4 r2       ; Write 70 to address 1024 (same cache index 0, different tag)
    
    ; Step 3: Read from address 0 again
    read 0 r3 r4        ; Read from address 0 into r4, should be 48
    read 1 r3 r5        ; Read from address 1 into r5, should be 49
    read 2 r3 r6        ; Read from address 2 into r6, should be 50
    read 3 r3 r7        ; Read from address 3 into r7, should be 51
    read 4 r3 r8        ; Read from address 4 into r8, should be 52
    read 5 r3 r9        ; Read from address 5 into r9, should be 53
    read 6 r3 r12       ; Read from address 6 into r12, should be 54
    read 7 r3 r13       ; Read from address 7 into r13, should be 55

    ; Send read back data over UART
    write 0 r10 r4       ; Send byte at address 0
    write 0 r10 r5       ; Send byte at address 1
    write 0 r10 r6       ; Send byte at address 2
    write 0 r10 r7       ; Send byte at address 3
    write 0 r10 r8       ; Send byte at address 4
    write 0 r10 r9       ; Send byte at address 5
    write 0 r10 r12      ; Send byte at address 6
    write 0 r10 r13      ; Send byte at address 7

    
    halt
