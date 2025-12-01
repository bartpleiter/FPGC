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


.data
Label_y:
  .dw 5
  .dw 6
  .dw 7

.code
Label_main:
  sub r13         11 r13
  write          3 r13 r14
  add r13          3 r14
   write 4 r14 r15

.data
Label_L2:
  .dw 1
  .dw 2
  .dw 3

.code
  add r14 -3 r6
  addr2reg Label_L2 r5
  load32 3 r4
  sub r13 16 r13
  savpc r15
  add r15 3 r15
  jump Label_L3
  sub r13 -16 r13
  read -1 r14 r1
  addr2reg Label_y r8
  add r8 1 r8
  read 0 r8 r8
  add r1 r8 r1
  sub r1 2 r1
  jump Label_L1
  load32 0 r1
Label_L1:
  read 4 r14 r15
  read 0 r14 r14
  add r13 11 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          8 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 4 r14 r15
Label_L4:
  read 0 r14 r14
  add r13 8 r13
  jumpr 0 r15

.code
Label_L3:
  or r0 r6 r2
  or r0 r6 r3
Label_L5:
  read 0 r5 r6
  add r5 1 r5
  sub r4 1 r4
  write 0 r3 r6
  add r3 1 r3
  beq r4 r0 2
  jump Label_L5
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
