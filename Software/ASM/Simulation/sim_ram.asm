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
Label_atoi:
  write 0 r13 r4
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  write -1 r14 r0
  load32 1 r1
  write -2 r14 r1
Label_L122:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 32 r1
  sltu r1 1 r1
  bne r1 r0 Label_L125
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 9 r1
  sltu r1 1 r1
Label_L125:
  sltu r0 r1 r1
  bne r1 r0 Label_L124
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 10 r1
  sltu r1 1 r1
Label_L124:
  beq r1 r0 Label_L123
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L122
Label_L123:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 45 r1
  bne r1 r0 Label_L126
  load32 -1 r1
  write -2 r14 r1
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L127
Label_L126:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 43 r1
  bne r1 r0 Label_L128
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
Label_L128:
Label_L127:
Label_L130:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  slt r1 48 r1
  xor r1 1 r1
  beq r1 r0 Label_L132
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  slt r1 58 r1
Label_L132:
  beq r1 r0 Label_L131
  read -1 r14 r1
  load32 10 r8
  mults r1 r8 r1
  read 2 r14 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  sub r8 48 r8
  add r1 r8 r1
  write -1 r14 r1
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L130
Label_L131:
  read -2 r14 r1
  read -1 r14 r8
  mults r1 r8 r1
Label_L121:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_utoa:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
  ;write 1 r14 r15

.data
Label_L134:
  .dsw "0123456789abcdef"
  .dw 0

.code

.data
Label_L135:
  .dsw "0123456789ABCDEF"
  .dw 0

.code
  read 5 r14 r1
  bne r1 r0 Label_L136
  addr2reg Label_L134 r1
  jump Label_L137
Label_L136:
  addr2reg Label_L135 r1
Label_L137:
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
  read 3 r14 r1
  write -3 r14 r1
Label_L138:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  read -1 r14 r8
  read 2 r14 r9
  read 4 r14 r10
  modu r9 r10 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  read 4 r14 r1
  read 2 r14 r12
  divu r12 r1 r1
  write 2 r14 r1
Label_L139:
  read 2 r14 r1
  bne r1 r0 Label_L138
Label_L140:
  read -2 r14 r1
  write 0 r1 r0
  read -2 r14 r1
  sub r1 1 r1
  write -2 r14 r1
  sub r1 -1 r1
Label_L141:
  read -3 r14 r1
  read -2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L142
  read -3 r14 r1
  read 0 r1 r1
  write -4 r14 r1
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  read -2 r14 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
  read -2 r14 r1
  sub r1 1 r1
  write -2 r14 r1
  sub r1 -1 r1
  read -4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  jump Label_L141
Label_L142:
  read 3 r14 r1
Label_L133:
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_itoa:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 3 r14 r1
  write -1 r14 r1
  read 2 r14 r1
  slt r1 r0 r1
  beq r1 r0 Label_L146
  read 4 r14 r1
  xor r1 10 r1
  sltu r1 1 r1
Label_L146:
  beq r1 r0 Label_L144
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  load32 45 r8
  write 0 r1 r8
  or r8 r0 r1
  read 2 r14 r1
  sub r0 r1 r1
  write 2 r14 r1
Label_L144:
  load32 0 r7
  read 4 r14 r6
  read -1 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_utoa
  sub r13 -4 r13
  read 3 r14 r1
Label_L143:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_abs:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L149
  read 2 r14 r1
  jump Label_L150
Label_L149:
  read 2 r14 r1
  sub r0 r1 r1
Label_L150:
Label_L148:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_labs:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L152
  read 2 r14 r1
  jump Label_L153
Label_L152:
  read 2 r14 r1
  sub r0 r1 r1
Label_L153:
Label_L151:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.data
Label_stdlib_rand_seed:
  .dw 1

.code
Label_rand:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_stdlib_rand_seed r11
  read 0 r11 r1
  load32 1103515245 r8
  mults r1 r8 r1
  add r1 12345 r1
  addr2reg Label_stdlib_rand_seed r11
  write 0 r11 r1
  addr2reg Label_stdlib_rand_seed r11
  read 0 r11 r1
  shiftr r1 16 r1
  and r1 32767 r1
Label_L154:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_srand:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  addr2reg Label_stdlib_rand_seed r11
  write 0 r11 r1
Label_L156:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_swap_words:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  write -1 r14 r0
Label_L158:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L161
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -2 r14 r1
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 3 r14 r8
  read -1 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L159:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L158
Label_L161:
Label_L157:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_quicksort_internal:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  pop r11
  write 4 r13 r11
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
   write 1 r14 r15
  read 3 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  bne r1 r0 Label_L163
  jump Label_L162
Label_L163:
  read 2 r14 r1
  read 4 r14 r8
  read 5 r14 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -3 r14 r1
  read 3 r14 r1
  write -1 r14 r1
  read 3 r14 r1
  write -2 r14 r1
Label_L165:
  read -2 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L168
  read -3 r14 r5
  read 2 r14 r4
  push r4
; Stack depth: 1
  read -2 r14 r4
  push r4
; Stack depth: 2
  read 5 r14 r4
  pop r11
; Stack depth: 1
  mults r11 r4 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  read 6 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jumpr 0 r1
  sub r13 -4 r13
  bgts r1 r0 Label_L169
  read 5 r14 r6
  read 2 r14 r5
  push r5
; Stack depth: 1
  read -2 r14 r5
  push r5
; Stack depth: 2
  read 5 r14 r5
  pop r11
; Stack depth: 1
  mults r11 r5 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  read 2 r14 r4
  push r4
; Stack depth: 1
  read -1 r14 r4
  push r4
; Stack depth: 2
  read 5 r14 r4
  pop r11
; Stack depth: 1
  mults r11 r4 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_swap_words
  sub r13 -4 r13
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
Label_L169:
Label_L166:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L165
Label_L168:
  read 5 r14 r6
  read -3 r14 r5
  read 2 r14 r4
  push r4
; Stack depth: 1
  read -1 r14 r4
  push r4
; Stack depth: 2
  read 5 r14 r4
  pop r11
; Stack depth: 1
  mults r11 r4 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_swap_words
  sub r13 -4 r13
  read -1 r14 r1
  beq r1 r0 Label_L171
  read 6 r14 r1
  push r1
; Stack depth: 1
  read 5 r14 r7
  read -1 r14 r6
  sub r6 1 r6
  read 3 r14 r5
  read 2 r14 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_quicksort_internal
  sub r13 -5 r13
Label_L171:
  read 6 r14 r1
  push r1
; Stack depth: 1
  read 5 r14 r7
  read 4 r14 r6
  read -1 r14 r5
  add r5 1 r5
  read 2 r14 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_quicksort_internal
  sub r13 -5 r13
Label_L162:
  read 1 r14 r15
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_qsort:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 3 r14 r1
  sltu r1 2 r1
  beq r1 r0 Label_L174
  jump Label_L173
Label_L174:
  read 5 r14 r1
  push r1
; Stack depth: 1
  read 4 r14 r7
  read 3 r14 r6
  sub r6 1 r6
  load32 0 r5
  read 2 r14 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_quicksort_internal
  sub r13 -5 r13
Label_L173:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_bsearch:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  pop r11
  write 4 r13 r11
  sub r13          7 r13
  write          5 r13 r14
  add r13          5 r14
   write 1 r14 r15
  write -1 r14 r0
  read 4 r14 r1
  write -2 r14 r1
  read 3 r14 r1
  write -4 r14 r1
Label_L179:
  read -1 r14 r1
  read -2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L180
  read -1 r14 r1
  read -2 r14 r8
  read -1 r14 r9
  sub r8 r9 r8
  shiftr r8 1 r8
  add r1 r8 r1
  write -3 r14 r1
  read -4 r14 r5
  push r5
; Stack depth: 1
  read -3 r14 r5
  push r5
; Stack depth: 2
  read 5 r14 r5
  pop r11
; Stack depth: 1
  mults r11 r5 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  read 2 r14 r4
  read 6 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jumpr 0 r1
  sub r13 -4 r13
  write -5 r14 r1
  read -5 r14 r1
  bne r1 r0 Label_L181
  read -4 r14 r1
  read -3 r14 r8
  read 5 r14 r9
  mults r8 r9 r8
  add r1 r8 r1
  jump Label_L177
  jump Label_L182
Label_L181:
  read -5 r14 r1
  bges r1 r0 Label_L184
  read -3 r14 r1
  write -2 r14 r1
  jump Label_L185
Label_L184:
  read -3 r14 r1
  add r1 1 r1
  write -1 r14 r1
Label_L185:
Label_L182:
  jump Label_L179
Label_L180:
  load32 0 r1
Label_L177:
  read 1 r14 r15
  read 0 r14 r14
  add r13 7 r13
  jumpr 0 r15

.code
Label_int_min:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  read 3 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L188
  read 2 r14 r1
  jump Label_L187
Label_L188:
  read 3 r14 r1
Label_L187:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_int_max:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  read 3 r14 r8
  slt r8 r1 r1
  beq r1 r0 Label_L191
  read 2 r14 r1
  jump Label_L190
Label_L191:
  read 3 r14 r1
Label_L190:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_int_clamp:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  read 3 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L194
  read 3 r14 r1
  jump Label_L193
Label_L194:
  read 2 r14 r1
  read 4 r14 r8
  slt r8 r1 r1
  beq r1 r0 Label_L196
  read 4 r14 r1
  jump Label_L193
Label_L196:
  read 2 r14 r1
Label_L193:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.bss
Label_rx_buffer:
  .dw 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

.data
Label_rx_head:
  .dw 0

.data
Label_rx_tail:
  .dw 0

.data
Label_rx_overflow_flag:
  .dw 0

.code
Label_rx_count:
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
  ;write 1 r14 r15
  addr2reg Label_rx_head r11
  read 0 r11 r1
  write -1 r14 r1
  addr2reg Label_rx_tail r11
  read 0 r11 r1
  write -2 r14 r1
  read -1 r14 r1
  read -2 r14 r8
  slt r1 r8 r1
  bne r1 r0 Label_L199
  read -1 r14 r1
  read -2 r14 r8
  sub r1 r8 r1
  jump Label_L198
Label_L199:
  load32 64 r1
  read -2 r14 r8
  sub r1 r8 r1
  read -1 r14 r8
  add r1 r8 r1
Label_L198:
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_uart_init:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_rx_head r11
  write 0 r11 r0
  addr2reg Label_rx_tail r11
  write 0 r11 r0
  addr2reg Label_rx_overflow_flag r11
  write 0 r11 r0
Label_L201:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_putchar:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  load32 117440512 r1
  write -1 r14 r1
  read -1 r14 r1
  read 2 r14 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L202:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_uart_puts:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L206
  jump Label_L205
Label_L206:
Label_L209:
  read 2 r14 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L210
  read 2 r14 r4
  read 0 r4 r4
  shiftl r4 24 r4
  shiftrs r4 24 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13
  read 2 r14 r1
  add r1 1 r1
  write 2 r14 r1
  add r1 -1 r1
  jump Label_L209
Label_L210:
Label_L205:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_putint:
  write 0 r13 r4
  sub r13         14 r13
  write         12 r13 r14
  add r13         12 r14
   write 1 r14 r15
  load32 10 r6
  add r14 -12 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_itoa
  sub r13 -4 r13
  add r14 -12 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
Label_L211:
  read 1 r14 r15
  read 0 r14 r14
  add r13 14 r13
  jumpr 0 r15

.code
Label_uart_puthex:
  write 0 r13 r4
  write 1 r13 r5
  sub r13         11 r13
  write          9 r13 r14
  add r13          9 r14
   write 1 r14 r15
  read 3 r14 r1
  beq r1 r0 Label_L213

.rdata
Label_L215:
  .dsw "0x"
  .dw 0

.code
  addr2reg Label_L215 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
Label_L213:
  load32 16 r6
  add r14 -9 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_itoa
  sub r13 -4 r13
  add r14 -9 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
Label_L212:
  read 1 r14 r15
  read 0 r14 r14
  add r13 11 r13
  jumpr 0 r15

.code
Label_uart_write:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L217
  jump Label_L216
Label_L217:
  write -1 r14 r0
Label_L220:
  read -1 r14 r1
  read 3 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L223
  read 2 r14 r4
  push r4
; Stack depth: 1
  read -1 r14 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  read 0 r4 r4
  shiftl r4 24 r4
  shiftrs r4 24 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13
Label_L221:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L220
Label_L223:
Label_L216:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_uart_isr_handler:
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
  ;write 1 r14 r15
  load32 97 r1
  write -1 r14 r1
  read -1 r14 r1
  read 0 r1 r1
  write -2 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  addr2reg Label_rx_head r11
  read 0 r11 r1
  add r1 1 r1
  load32 64 r8
  mods r1 r8 r1
  write -3 r14 r1
  read -3 r14 r1
  addr2reg Label_rx_tail r11
  read 0 r11 r8
  bne r1 r8 Label_L226
  load32 1 r1
  addr2reg Label_rx_overflow_flag r11
  write 0 r11 r1
  jump Label_L224
Label_L226:
  addr2reg Label_rx_buffer r1
  addr2reg Label_rx_head r11
  read 0 r11 r8
  add r1 r8 r1
  read -2 r14 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -3 r14 r1
  addr2reg Label_rx_head r11
  write 0 r11 r1
Label_L224:
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_uart_available:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_rx_count
  sub r13 -4 r13
Label_L228:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_read:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_rx_head r11
  read 0 r11 r1
  addr2reg Label_rx_tail r11
  read 0 r11 r8
  bne r1 r8 Label_L230
  load32 -1 r1
  jump Label_L229
Label_L230:
  addr2reg Label_rx_buffer r1
  addr2reg Label_rx_tail r11
  read 0 r11 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  write -1 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  addr2reg Label_rx_tail r11
  read 0 r11 r1
  add r1 1 r1
  load32 64 r8
  mods r1 r8 r1
  addr2reg Label_rx_tail r11
  write 0 r11 r1
  read -1 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
Label_L229:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_uart_peek:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_rx_head r11
  read 0 r11 r1
  addr2reg Label_rx_tail r11
  read 0 r11 r8
  bne r1 r8 Label_L235
  load32 -1 r1
  jump Label_L234
Label_L235:
  addr2reg Label_rx_buffer r1
  addr2reg Label_rx_tail r11
  read 0 r11 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
Label_L234:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_read_bytes:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  write -1 r14 r0
  read 2 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L243
  read 3 r14 r1
  slt r1 1 r1
Label_L243:
  beq r1 r0 Label_L240
  load32 0 r1
  jump Label_L239
Label_L240:
Label_L244:
  read -1 r14 r1
  read 3 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L245
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_read
  sub r13 -4 r13
  write -2 r14 r1
  read -2 r14 r1
  bges r1 r0 Label_L246
  jump Label_L245
Label_L246:
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L244
Label_L245:
  read -1 r14 r1
Label_L239:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_uart_read_until:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  write -1 r14 r0
  read 2 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L253
  read 3 r14 r1
  slt r1 1 r1
Label_L253:
  beq r1 r0 Label_L250
  load32 0 r1
  jump Label_L249
Label_L250:
Label_L254:
  read -1 r14 r1
  read 3 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L255
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_read
  sub r13 -4 r13
  write -2 r14 r1
  read -2 r14 r1
  bges r1 r0 Label_L256
  jump Label_L255
Label_L256:
  read 2 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  read -2 r14 r1
  read 4 r14 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  bne r1 r8 Label_L259
  jump Label_L255
Label_L259:
  jump Label_L254
Label_L255:
  read -1 r14 r1
Label_L249:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_uart_read_line:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  load32 10 r6
  read 3 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_read_until
  sub r13 -4 r13
Label_L262:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_flush_rx:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_rx_head r11
  write 0 r11 r0
  addr2reg Label_rx_tail r11
  write 0 r11 r0
  addr2reg Label_rx_overflow_flag r11
  write 0 r11 r0
Label_L263:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_uart_rx_overflow:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_rx_overflow_flag r11
  read 0 r11 r1
  write -1 r14 r1
  addr2reg Label_rx_overflow_flag r11
  write 0 r11 r0
  read -1 r14 r1
Label_L264:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.bss
Label_timer_state:
  .dw 0 0 0 0 0 0 0 0 0

.data
Label_delay_complete:
  .dw 0

.code
Label_timer_val_addr:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L266
  load32 117440514 r1
  jump Label_L265
Label_L266:
  read 2 r14 r1
  xor r1 1 r1
  bne r1 r0 Label_L269
  load32 117440516 r1
  jump Label_L265
Label_L269:
  read 2 r14 r1
  xor r1 2 r1
  bne r1 r0 Label_L272
  load32 117440518 r1
  jump Label_L265
Label_L272:
  load32 117440514 r1
Label_L265:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_ctrl_addr:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L277
  load32 117440515 r1
  jump Label_L276
Label_L277:
  read 2 r14 r1
  xor r1 1 r1
  bne r1 r0 Label_L280
  load32 117440517 r1
  jump Label_L276
Label_L280:
  read 2 r14 r1
  xor r1 2 r1
  bne r1 r0 Label_L283
  load32 117440519 r1
  jump Label_L276
Label_L283:
  load32 117440515 r1
Label_L276:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_valid:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  slt r1 r0 r1
  xor r1 1 r1
  beq r1 r0 Label_L288
  read 2 r14 r1
  slt r1 3 r1
Label_L288:
Label_L287:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_init:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
Label_L290:
  read -1 r14 r1
  slt r1 3 r1
  beq r1 r0 Label_L293
  addr2reg Label_timer_state r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L291:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L290
Label_L293:
  addr2reg Label_delay_complete r11
  write 0 r11 r0
Label_L289:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_timer_set:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L296
  jump Label_L295
Label_L296:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_val_addr
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L295:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_timer_start:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L299
  jump Label_L298
Label_L299:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_ctrl_addr
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L298:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_timer_start_ms:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L302
  jump Label_L301
Label_L302:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_set
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_start
  sub r13 -4 r13
Label_L301:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_get_period:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L305
  load32 0 r1
  jump Label_L304
Label_L305:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  read 0 r1 r1
Label_L304:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_set_callback:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L308
  jump Label_L307
Label_L308:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L307:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_start_periodic:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L311
  jump Label_L310
Label_L311:
  read 3 r14 r1
  bne r1 r0 Label_L313
  jump Label_L310
Label_L313:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_set
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_start
  sub r13 -4 r13
Label_L310:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_cancel:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L316
  jump Label_L315
Label_L316:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L315:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_is_active:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L320
  load32 0 r1
  jump Label_L319
Label_L320:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
Label_L319:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_timer_isr_handler:
  write 0 r13 r4
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_valid
  sub r13 -4 r13
  bne r1 r0 Label_L323
  jump Label_L322
Label_L323:
  read 2 r14 r1
  xor r1 2 r1
  bne r1 r0 Label_L325
  load32 1 r1
  addr2reg Label_delay_complete r11
  write 0 r11 r1
Label_L325:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L327
  jump Label_L322
Label_L327:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  read 0 r1 r1
  write -1 r14 r1
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  read 0 r1 r1
  write -2 r14 r1
  read -1 r14 r1
  beq r1 r0 Label_L329
  read 2 r14 r4
  read -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jumpr 0 r1
  sub r13 -4 r13
Label_L329:
  read -2 r14 r1
  beq r1 r0 Label_L332
  read -2 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_set
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_start
  sub r13 -4 r13
  jump Label_L333
Label_L332:
  addr2reg Label_timer_state r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L333:
Label_L322:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_delay:
  write 0 r13 r4
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
   write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L335
  jump Label_L334
Label_L335:
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 0 r1
  read 0 r1 r1
  write -1 r14 r1
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 1 r1
  read 0 r1 r1
  write -2 r14 r1
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 2 r1
  read 0 r1 r1
  write -3 r14 r1
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 2 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_delay_complete r11
  write 0 r11 r0
  read 2 r14 r5
  load32 2 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_set
  sub r13 -4 r13
  load32 2 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_timer_start
  sub r13 -4 r13
Label_L338:
  addr2reg Label_delay_complete r11
  read 0 r11 r1
  bne r1 r0 Label_L339
  jump Label_L338
Label_L339:
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 0 r1
  read -1 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 1 r1
  read -2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_timer_state r1
  add r1 6 r1
  add r1 2 r1
  read -3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L334:
  read 1 r14 r15
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_get_int_id:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
readintid r11      ; r11 = interrupt ID
write -1 r14 r11   ; Write to stack for return
  read -1 r14 r1
Label_L340:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_get_boot_mode:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  load32 117440537 r1
  write -1 r14 r1
  read -1 r14 r1
  read 0 r1 r1
Label_L341:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_get_micros:
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  load32 117440538 r1
  write -1 r14 r1
  read -1 r14 r1
  read 0 r1 r1
Label_L343:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_main:
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_init
  sub r13 -4 r13

.rdata
Label_L346:
  .dsw "UART Test\012"
  .dw 0

.code
  addr2reg Label_L346 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
  write -2 r14 r0
Label_L347:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  read -2 r14 r1
  load32 10 r8
  mods r1 r8 r1
  bne r1 r0 Label_L349
  load32 46 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_isr_handler
  sub r13 -4 r13
  load32 95 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13
Label_L349:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_available
  sub r13 -4 r13
  bles r1 r0 Label_L351
  load32 120 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_read
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  blts r1 r0 Label_L353

.rdata
Label_L355:
  .dsw "Received byte: "
  .dw 0

.code
  addr2reg Label_L355 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
  read -1 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_putchar
  sub r13 -4 r13

.rdata
Label_L357:
  .dsw "\012"
  .dw 0

.code
  addr2reg Label_L357 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_uart_puts
  sub r13 -4 r13
Label_L353:
Label_L351:
  jump Label_L347
Label_L348:
  load32 0 r1
  jump Label_L345
  load32 0 r1
Label_L345:
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
Label_L358:
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
