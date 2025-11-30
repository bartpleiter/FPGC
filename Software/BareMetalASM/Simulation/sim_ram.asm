.code
; Setup stack and return function before jumping to Main of C program
Main:
  load32 0 r14            ; initialize base pointer address
  load32 0x77FFFF r13     ; initialize main stack address
  addr2reg Return_UART r1 ; get address of return function
  or r0 r1 r15            ; copy return addr to r15
  jump Label_main         ; jump to main of C program
                          ; should return to the address in r15
  halt                    ; should not get here

; Function that is called after Main of C program has returned
; Return value should be in r1
; Send it over UART and halt afterwards
Return_UART:
  load32 0x7000000 r2 ; r1 = 0x7000000 | UART tx
  write 0 r2 r1       ; write r2 over UART
  halt                ; halt


.code
Label_main:
  sub r13         12 r13
  write          4 r13 r14
  add r13          4 r14
  ;write 4 r14 r15
  load32 2 r1
  write -4 r14 r1
  read -4 r14 r1
  slt r1 2 r1
  beq r1 r0 Label_L2
  load32 5 r1
  write -4 r14 r1
  jump Label_L3
Label_L2:
  read -4 r14 r1
  xor r1 2 r1
  bne r1 r0 Label_L4
  load32 3 r1
  write -4 r14 r1
  jump Label_L5
Label_L4:
  load32 4 r1
  write -4 r14 r1
Label_L5:
Label_L3:
  read -4 r14 r1
  jump Label_L1
  load32 0 r1
Label_L1:
  read 0 r14 r14
  add r13 12 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          8 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 4 r14 r15
Label_L6:
  read 0 r14 r14
  add r13 8 r13
  jumpr 0 r15

.code
; Interrupt handlers
; Has some administration before jumping to interrupt function
; To prevent interfering with other stacks, they have their own stack
; Also, all registers have to be backed up and restored to hardware stack
; A return function has to be put on the stack as wel that the C code interrupt handler
; will jump to when it is done

Int:
  push r1
  push r2
  push r3
  push r4
  push r5
  push r6
  push r7
  push r8
  push r9
  push r10
  push r11
  push r12
  push r13
  push r14
  push r15

  load32 0x7FFFFF r13     ; initialize (BDOS) int stack address
  load32 0 r14            ; initialize base pointer address
  addr2reg Return_Interrupt r1 ; get address of return function
  or r0 r1 r15            ; copy return addr to r15
  jump Label_interrupt    ; jump to interrupt handler of C program
                            ; should return to the address we just put on the stack
  halt                    ; should not get here


; Function that is called after the interrupt handler from C has returned
; Restores all registers and issues RETI instruction to continue from original code
Return_Interrupt:
  pop r15
  pop r14
  pop r13
  pop r12
  pop r11
  pop r10
  pop r9
  pop r8
  pop r7
  pop r6
  pop r5
  pop r4
  pop r3
  pop r2
  pop r1

  reti        ; return from interrrupt
  halt        ; should not get here
