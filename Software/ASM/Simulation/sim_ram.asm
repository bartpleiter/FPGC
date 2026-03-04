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


.bss
Label_max_iter:
  .dw 0

.code
Label_mandelbrot_pixel:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
addr2reg Label_max_iter r11
read 0 r11 r4
fld r0 r0 r2
fld r0 r0 r3
or r0 r0 r5
beq r4 r0 Label_mbrot_asm_set
Label_mbrot_asm_loop:
fmul r2 r2 r4       ; f4 = z_re^2
fmul r3 r3 r5       ; f5 = z_im^2
fmul r2 r3 r6       ; f6 = z_re * z_im
fsub r4 r5 r2       ; f2 = z_re^2 - z_im^2
fadd r2 r0 r2       ; f2 += c_re  (new z_re)
fadd r6 r6 r3       ; f3 = 2 * z_re * z_im
fadd r3 r1 r3       ; f3 += c_im  (new z_im)
fadd r4 r5 r4       ; f4 = z_re^2 + z_im^2 = |z|^2
fsthi r4 r0 r1      ; r1 = integer part of |z|^2
sltu r1 4 r1        ; r1 = 1 if mag < 4 (not escaped)
beq r1 r0 Label_mbrot_asm_escaped
add r5 1 r5
slt r5 r4 r1        ; iter < max_iter?
bne r1 r0 Label_mbrot_asm_loop
Label_mbrot_asm_set:
write -1 r14 r0     ; retval = 0
jump Label_mbrot_asm_done
Label_mbrot_asm_escaped:
add r5 1 r1         ; r1 = iter + 1
write -1 r14 r1     ; retval = iter + 1
Label_mbrot_asm_done:
  read -1 r14 r1
Label_L1:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_main:
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  load32 32 r1
  addr2reg Label_max_iter r11
  write 0 r11 r1
  load32 0 r1
  load32 0 r8
  fld r8 r1 r0
  load32 0 r1
  load32 0 r8
  fld r8 r1 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_mandelbrot_pixel
  sub r13 -4 r13
  write -1 r14 r1
  load32 0 r1
  load32 2 r8
  fld r8 r1 r0
  load32 0 r1
  load32 0 r8
  fld r8 r1 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_mandelbrot_pixel
  sub r13 -4 r13
  write -2 r14 r1
  read -2 r14 r1
  shiftl r1 8 r1
  read -1 r14 r8
  add r1 r8 r1
  jump Label_L2
  load32 0 r1
Label_L2:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
Label_L3:
  read 0 r14 r14
  add r13 2 r13
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

  load32 0x7FFFFF r13     ; initialize int stack address
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
