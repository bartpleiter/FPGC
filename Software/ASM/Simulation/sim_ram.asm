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
Label_gfx_cursor_x:
  .dw 0

.data
Label_gfx_cursor_y:
  .dw 0

.data
Label_gfx_saved_cursor_x:
  .dw 0

.data
Label_gfx_saved_cursor_y:
  .dw 0

.data
Label_gfx_fg_color:
  .dw 0

.data
Label_gfx_bg_color:
  .dw 0

.data
Label_gfx_cursor_visible:
  .dw 1

.data
Label_gfx_scroll_top:
  .dw 0

.data
Label_gfx_scroll_bottom:
  .dw 24

.code
Label_GFX_init:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r0
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r0
  addr2reg Label_gfx_fg_color r11
  write 0 r11 r0
  addr2reg Label_gfx_scroll_top r11
  write 0 r11 r0
  load32 24 r1
  addr2reg Label_gfx_scroll_bottom r11
  write 0 r11 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_clear
  sub r13 -4 r13
Label_L1:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_copy_palette_table:
  write 0 r13 r4
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  load32 126878720 r1
  write -2 r14 r1
  load32 126878720 r1
  write -3 r14 r1
  write -1 r14 r0
Label_L4:
  read -1 r14 r1
  slt r1 32 r1
  beq r1 r0 Label_L7
  read -3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 2 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L5:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L4
Label_L7:
Label_L2:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_GFX_debug_uart_putchar:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  load32 117440512 r1
  write -1 r14 r1
  read -1 r14 r1
  read 2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L8:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_GFX_copy_pattern_table:
  write 0 r13 r4
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  load32 126877696 r1
  write -2 r14 r1
  load32 126877696 r1
  write -3 r14 r1
  write -1 r14 r0
Label_L12:
  read -1 r14 r1
  slt r1 1024 r1
  beq r1 r0 Label_L15
  read -3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 2 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L13:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L12
Label_L15:
Label_L10:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_GFX_cursor_set:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  sltu r1 r0 r1
  xor r1 1 r1
  beq r1 r0 Label_L19
  read 2 r14 r1
  sltu r1 40 r1
Label_L19:
  beq r1 r0 Label_L17
  read 2 r14 r1
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r1
Label_L17:
  read 3 r14 r1
  sltu r1 r0 r1
  xor r1 1 r1
  beq r1 r0 Label_L22
  read 3 r14 r1
  sltu r1 25 r1
Label_L22:
  beq r1 r0 Label_L20
  read 3 r14 r1
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r1
Label_L20:
Label_L16:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_cursor_get:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r1
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L23:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_cursor_save:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  addr2reg Label_gfx_saved_cursor_x r11
  write 0 r11 r1
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r1
  addr2reg Label_gfx_saved_cursor_y r11
  write 0 r11 r1
Label_L24:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_cursor_restore:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_gfx_saved_cursor_x r11
  read 0 r11 r1
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r1
  addr2reg Label_gfx_saved_cursor_y r11
  read 0 r11 r1
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r1
Label_L25:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_putchar_at:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 3 r14 r1
  sltu r1 r0 r1
  bne r1 r0 Label_L31
  read 3 r14 r1
  sltu r1 40 r1
  xor r1 1 r1
Label_L31:
  sltu r0 r1 r1
  bne r1 r0 Label_L30
  read 4 r14 r1
  sltu r1 r0 r1
Label_L30:
  sltu r0 r1 r1
  bne r1 r0 Label_L29
  read 4 r14 r1
  sltu r1 25 r1
  xor r1 1 r1
Label_L29:
  beq r1 r0 Label_L27
  jump Label_L26
Label_L27:
  read 4 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127930368 r12
  add r1 r12 r1
  pop r12
  read 3 r14 r8
  add r1 r8 r1
  write -1 r14 r1
  read 4 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127932416 r12
  add r1 r12 r1
  pop r12
  read 3 r14 r8
  add r1 r8 r1
  write -2 r14 r1
  read -1 r14 r1
  read 2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -2 r14 r1
  addr2reg Label_gfx_fg_color r11
  read 0 r11 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L26:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_GFX_putchar:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r1
  xor r1 10 r1
  bne r1 r0 Label_L36
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r0
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r1
  add r1 1 r1
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r1
  add r1 -1 r1
  jump Label_L37
Label_L36:
  read 2 r14 r1
  xor r1 13 r1
  bne r1 r0 Label_L38
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r0
  jump Label_L39
Label_L38:
  read 2 r14 r1
  xor r1 9 r1
  bne r1 r0 Label_L40
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  add r1 4 r1
  and r1 -4 r1
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r1
  jump Label_L41
Label_L40:
  read 2 r14 r1
  xor r1 8 r1
  bne r1 r0 Label_L42
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  beq r1 r0 Label_L44
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  sub r1 1 r1
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r1
  sub r1 -1 r1
Label_L44:
  jump Label_L43
Label_L42:
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r6
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_putchar_at
  sub r13 -4 r13
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  add r1 1 r1
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r1
  add r1 -1 r1
Label_L43:
Label_L41:
Label_L39:
Label_L37:
  addr2reg Label_gfx_cursor_x r11
  read 0 r11 r1
  sltu r1 40 r1
  bne r1 r0 Label_L46
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r0
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r1
  add r1 1 r1
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r1
  add r1 -1 r1
Label_L46:
  addr2reg Label_gfx_cursor_y r11
  read 0 r11 r1
  addr2reg Label_gfx_scroll_bottom r11
  read 0 r11 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L48
  load32 1 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_scroll_up
  sub r13 -4 r13
  addr2reg Label_gfx_scroll_bottom r11
  read 0 r11 r1
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r1
Label_L48:
Label_L35:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_puts:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
Label_L51:
  read 2 r14 r1
  read 0 r1 r1
  beq r1 r0 Label_L52
  read 2 r14 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_putchar
  sub r13 -4 r13
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  load32 66 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_debug_uart_putchar
  sub r13 -4 r13
  jump Label_L51
Label_L52:
Label_L50:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_GFX_clear:
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
  ;write 1 r14 r15
  load32 127930368 r1
  write -2 r14 r1
  load32 127932416 r1
  write -3 r14 r1
  load32 1000 r1
  write -4 r14 r1
  write -1 r14 r0
Label_L54:
  read -1 r14 r1
  read -4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L57
  read -2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read -3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  addr2reg Label_gfx_fg_color r11
  read 0 r11 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L55:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L54
Label_L57:
  addr2reg Label_gfx_cursor_x r11
  write 0 r11 r0
  addr2reg Label_gfx_cursor_y r11
  write 0 r11 r0
Label_L53:
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_GFX_clear_line:
  write 0 r13 r4
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  read 2 r14 r1
  sltu r1 r0 r1
  bne r1 r0 Label_L63
  read 2 r14 r1
  sltu r1 25 r1
  xor r1 1 r1
Label_L63:
  beq r1 r0 Label_L61
  jump Label_L60
Label_L61:
  read 2 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127930368 r12
  add r1 r12 r1
  pop r12
  write -2 r14 r1
  read 2 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127932416 r12
  add r1 r12 r1
  pop r12
  write -3 r14 r1
  write -1 r14 r0
Label_L64:
  read -1 r14 r1
  sltu r1 40 r1
  beq r1 r0 Label_L67
  read -2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read -3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  addr2reg Label_gfx_fg_color r11
  read 0 r11 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L65:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L64
Label_L67:
Label_L60:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_GFX_scroll_up:
  write 0 r13 r4
  sub r13         10 r13
  write          8 r13 r14
  add r13          8 r14
   write 1 r14 r15
  read 2 r14 r1
  sltu r1 1 r1
  beq r1 r0 Label_L71
  jump Label_L70
Label_L71:
  addr2reg Label_gfx_scroll_top r11
  read 0 r11 r1
  write -1 r14 r1
Label_L73:
  read -1 r14 r1
  addr2reg Label_gfx_scroll_bottom r11
  read 0 r11 r8
  read 2 r14 r9
  sub r8 r9 r8
  sltu r8 r1 r1
  bne r1 r0 Label_L76
  read -1 r14 r1
  read 2 r14 r8
  add r1 r8 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127930368 r12
  add r1 r12 r1
  pop r12
  write -3 r14 r1
  read -1 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127930368 r12
  add r1 r12 r1
  pop r12
  write -4 r14 r1
  read -1 r14 r1
  read 2 r14 r8
  add r1 r8 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127932416 r12
  add r1 r12 r1
  pop r12
  write -5 r14 r1
  read -1 r14 r1
  load32 40 r8
  mults r1 r8 r1
  push r12
  load32 127932416 r12
  add r1 r12 r1
  pop r12
  write -6 r14 r1
  write -2 r14 r0
Label_L77:
  read -2 r14 r1
  sltu r1 40 r1
  beq r1 r0 Label_L80
  read -3 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -7 r14 r1
  read -5 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -8 r14 r1
  read -4 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read -7 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -6 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read -8 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L78:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L77
Label_L80:
Label_L74:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L73
Label_L76:
  addr2reg Label_gfx_scroll_bottom r11
  read 0 r11 r1
  read 2 r14 r8
  sub r1 r8 r1
  add r1 1 r1
  write -1 r14 r1
Label_L85:
  read -1 r14 r1
  addr2reg Label_gfx_scroll_bottom r11
  read 0 r11 r8
  sltu r8 r1 r1
  bne r1 r0 Label_L88
  read -1 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_clear_line
  sub r13 -4 r13
Label_L86:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L85
Label_L88:
Label_L70:
  read 1 r14 r15
  read 0 r14 r14
  add r13 10 r13
  jumpr 0 r15

.code
Label_main:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15

.rdata
Label_L90:
  .dsw "abcd"
  .dw 0

.code
  addr2reg Label_L90 r1
  write -1 r14 r1
  read -1 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_puts
  sub r13 -4 r13
  load32 55 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_GFX_debug_uart_putchar
  sub r13 -4 r13
  load32 57 r1
  jump Label_L89
  load32 0 r1
Label_L89:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
Label_L91:
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
