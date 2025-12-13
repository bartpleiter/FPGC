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
Label_gpu_write_pixel_data:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 2 r14 r1
  sltu r1 320 r1
  bne r1 r0 Label_L2
  read 2 r14 r1
  load32 320 r8
  modu r1 r8 r1
  write 2 r14 r1
Label_L2:
  read 3 r14 r1
  sltu r1 240 r1
  bne r1 r0 Label_L4
  read 3 r14 r1
  load32 240 r8
  modu r1 r8 r1
  write 3 r14 r1
Label_L4:
  load32 128974848 r1
  write -1 r14 r1
  read 3 r14 r1
  load32 320 r8
  mults r1 r8 r1
  read 2 r14 r8
  add r1 r8 r1
  write -2 r14 r1
  read -1 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L1:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_fb_draw_line:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  pop r11
  write 4 r13 r11
  sub r13          8 r13
  write          6 r13 r14
  add r13          6 r14
   write 1 r14 r15
  read 4 r14 r1
  read 2 r14 r8
  slt r8 r1 r1
  bne r1 r0 Label_L8
  read 2 r14 r1
  read 4 r14 r8
  sub r1 r8 r1
  jump Label_L9
Label_L8:
  read 4 r14 r1
  read 2 r14 r8
  sub r1 r8 r1
Label_L9:
  write -1 r14 r1
  read 5 r14 r1
  read 3 r14 r8
  slt r8 r1 r1
  bne r1 r0 Label_L10
  read 3 r14 r1
  read 5 r14 r8
  sub r1 r8 r1
  jump Label_L11
Label_L10:
  read 5 r14 r1
  read 3 r14 r8
  sub r1 r8 r1
Label_L11:
  write -2 r14 r1
  read 2 r14 r1
  read 4 r14 r8
  slt r1 r8 r1
  bne r1 r0 Label_L12
  load32 -1 r1
  jump Label_L13
Label_L12:
  load32 1 r1
Label_L13:
  write -3 r14 r1
  read 3 r14 r1
  read 5 r14 r8
  slt r1 r8 r1
  bne r1 r0 Label_L14
  load32 -1 r1
  jump Label_L15
Label_L14:
  load32 1 r1
Label_L15:
  write -4 r14 r1
  read -1 r14 r1
  read -2 r14 r8
  sub r1 r8 r1
  write -5 r14 r1
Label_L16:
  read 6 r14 r6
  read 3 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_gpu_write_pixel_data
  sub r13 -4 r13
  read 2 r14 r1
  read 4 r14 r8
  xor r1 r8 r1
  sltu r1 1 r1
  beq r1 r0 Label_L20
  read 3 r14 r1
  read 5 r14 r8
  xor r1 r8 r1
  sltu r1 1 r1
Label_L20:
  beq r1 r0 Label_L18
  jump Label_L17
Label_L18:
  read -5 r14 r1
  shiftl r1 1 r1
  write -6 r14 r1
  read -6 r14 r1
  read -2 r14 r8
  sub r0 r8 r8
  slt r8 r1 r1
  beq r1 r0 Label_L21
  read -2 r14 r1
  read -5 r14 r12
  sub r12 r1 r1
  write -5 r14 r1
  read -3 r14 r1
  read 2 r14 r12
  add r12 r1 r1
  write 2 r14 r1
Label_L21:
  read -6 r14 r1
  read -1 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L23
  read -1 r14 r1
  read -5 r14 r12
  add r12 r1 r1
  write -5 r14 r1
  read -4 r14 r1
  read 3 r14 r12
  add r12 r1 r1
  write 3 r14 r1
Label_L23:
  jump Label_L16
Label_L17:
Label_L7:
  read 1 r14 r15
  read 0 r14 r14
  add r13 8 r13
  jumpr 0 r15

.code
Label_fb_fill_rect:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  pop r11
  write 4 r13 r11
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  write -2 r14 r0
Label_L26:
  read -2 r14 r1
  read 5 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L29
  write -1 r14 r0
Label_L30:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L33
  read 6 r14 r6
  read 3 r14 r5
  push r5
; Stack depth: 1
  read -2 r14 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  read 2 r14 r4
  push r4
; Stack depth: 1
  read -1 r14 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_gpu_write_pixel_data
  sub r13 -4 r13
Label_L31:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L30
Label_L33:
Label_L27:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L26
Label_L29:
Label_L25:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_main:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  load32 170 r1
  push r1
; Stack depth: 1
  load32 3 r7
  load32 3 r6
  load32 10 r5
  load32 10 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_fb_fill_rect
  sub r13 -5 r13
  load32 3 r1
  push r1
; Stack depth: 1
  load32 10 r7
  load32 10 r6
  load32 12 r5
  load32 12 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_fb_draw_line
  sub r13 -5 r13
  load32 57 r1
  jump Label_L34
  load32 0 r1
Label_L34:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
Label_L35:
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
