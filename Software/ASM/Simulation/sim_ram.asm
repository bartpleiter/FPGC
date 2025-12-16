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
Label_memcpy:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
  write -3 r14 r0
Label_L4:
  read -3 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L7
  read -1 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  read -3 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L5:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L4
Label_L7:
  read 2 r14 r1
Label_L1:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_memset:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
  write -2 r14 r0
Label_L10:
  read -2 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L13
  read -1 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L11:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L10
Label_L13:
  read 2 r14 r1
Label_L8:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_memmove:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
  read -1 r14 r1
  read -2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L17
  write -3 r14 r0
Label_L19:
  read -3 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L22
  read -1 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  read -3 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L20:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L19
Label_L22:
  jump Label_L18
Label_L17:
  read -1 r14 r1
  read -2 r14 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L23
  read 4 r14 r1
  write -3 r14 r1
Label_L25:
  read -3 r14 r1
  beq r1 r0 Label_L28
  read -1 r14 r1
  read -3 r14 r8
  sub r8 1 r8
  add r1 r8 r1
  read -2 r14 r8
  read -3 r14 r9
  sub r9 1 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L26:
  read -3 r14 r1
  sub r1 1 r1
  write -3 r14 r1
  sub r1 -1 r1
  jump Label_L25
Label_L28:
Label_L23:
Label_L18:
  read 2 r14 r1
Label_L14:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_memcmp:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
  write -3 r14 r0
Label_L32:
  read -3 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L35
  read -1 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  read -2 r14 r8
  read -3 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L36
  load32 -1 r1
  jump Label_L29
Label_L36:
  read -1 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  read -2 r14 r8
  read -3 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L38
  load32 1 r1
  jump Label_L29
Label_L38:
Label_L33:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L32
Label_L35:
  load32 0 r1
Label_L29:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_strlen:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
Label_L41:
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L42
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L41
Label_L42:
  read -1 r14 r1
Label_L40:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strcpy:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
Label_L44:
  read 3 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L45
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  read 3 r14 r8
  add r8 1 r8
  write 3 r14 r8
  add r8 -1 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  jump Label_L44
Label_L45:
  read -1 r14 r1
  write 0 r1 r0
  read 2 r14 r1
Label_L43:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strncpy:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
Label_L47:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L51
  read 3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
Label_L51:
  beq r1 r0 Label_L50
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 3 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
Label_L48:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L47
Label_L50:
Label_L52:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L55
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  write 0 r1 r0
Label_L53:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L52
Label_L55:
  read 2 r14 r1
Label_L46:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strcmp:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
Label_L57:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
  beq r1 r0 Label_L59
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  xor r1 r8 r1
  sltu r1 1 r1
Label_L59:
  beq r1 r0 Label_L58
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  read 3 r14 r1
  add r1 1 r1
  write 3 r14 r1
  add r1 -1 r1
  jump Label_L57
Label_L58:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  sub r1 r8 r1
Label_L56:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_strncmp:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
Label_L63:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L66
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  beq r1 r8 Label_L67
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  sub r1 r8 r1
  jump Label_L62
Label_L67:
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  bne r1 r0 Label_L71
  load32 0 r1
  jump Label_L62
Label_L71:
Label_L64:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L63
Label_L66:
  load32 0 r1
Label_L62:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strcat:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
Label_L74:
  read -1 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L75
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L74
Label_L75:
Label_L76:
  read 3 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L77
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  read 3 r14 r8
  add r8 1 r8
  write 3 r14 r8
  add r8 -1 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  jump Label_L76
Label_L77:
  read -1 r14 r1
  write 0 r1 r0
  read 2 r14 r1
Label_L73:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strncat:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 2 r14 r1
  write -1 r14 r1
Label_L79:
  read -1 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L80
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L79
Label_L80:
  write -2 r14 r0
Label_L81:
  read -2 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L85
  read 3 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
Label_L85:
  beq r1 r0 Label_L84
  read -1 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 3 r14 r8
  read -2 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
Label_L82:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L81
Label_L84:
  read -1 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read 2 r14 r1
Label_L78:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_strchr:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  read 3 r14 r1
  write -1 r14 r1
Label_L88:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L89
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -1 r14 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  bne r1 r8 Label_L90
  read 2 r14 r1
  jump Label_L86
Label_L90:
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L88
Label_L89:
  read -1 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  bne r1 r0 Label_L93
  read 2 r14 r1
  jump Label_L86
Label_L93:
  load32 0 r1
Label_L86:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_strrchr:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 3 r14 r1
  write -1 r14 r1
  write -2 r14 r0
Label_L100:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L101
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -1 r14 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  bne r1 r8 Label_L102
  read 2 r14 r1
  write -2 r14 r1
Label_L102:
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L100
Label_L101:
  read -1 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  bne r1 r0 Label_L104
  read 2 r14 r1
  jump Label_L97
Label_L104:
  read -2 r14 r1
Label_L97:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_strstr:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  read 3 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  bne r1 r0 Label_L109
  read 2 r14 r1
  jump Label_L108
Label_L109:
Label_L112:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L113
  read 2 r14 r1
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
Label_L114:
  read -1 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -2 r14 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  xor r1 r8 r1
  sltu r1 1 r1
  beq r1 r0 Label_L116
  read -2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
Label_L116:
  beq r1 r0 Label_L115
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L114
Label_L115:
  read -2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  bne r1 r0 Label_L117
  read 2 r14 r1
  jump Label_L108
Label_L117:
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L112
Label_L113:
  load32 0 r1
Label_L108:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_test_strlen:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  read 4 r14 r8
  add r1 r8 r1
Label_L121:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_main:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15

.rdata
Label_L123:
  .dsw "Hello"
  .dw 0

.code
  addr2reg Label_L123 r1
  write -1 r14 r1
  sub r13 1 r13
  sub r13 3 r13
  read -1 r14 r1
  push r1
; Stack depth: 1
  pop r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  push r1
; Stack depth: 2
  read -1 r14 r1
  push r1
; Stack depth: 3
  load32 0 r1
  push r1
; Stack depth: 4
  pop r4
  pop r5
  pop r6
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_test_strlen
  sub r13 -4 r13
  load32 10 r1
  jump Label_L122
  load32 0 r1
Label_L122:
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
Label_L124:
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
