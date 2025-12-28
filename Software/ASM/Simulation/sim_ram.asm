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
Label_spi_0_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000009 r11 ; r11 = SPI0 cs register
write 0 r11 r0       ; Set cs low
Label_L121:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_0_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000009 r11 ; r11 = SPI0 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L122:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_0_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x7000008 r11 ; r11 = SPI0 data register
write 0 r11 r4       ; Write data to SPI0
read 0 r11 r11       ; Read received data from SPI0
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L123:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_1_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x700000B r11 ; r11 = SPI1 cs register
write 0 r11 r0       ; Set cs low
Label_L124:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_1_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x700000B r11 ; r11 = SPI1 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L125:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_1_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x700000A r11 ; r11 = SPI1 data register
write 0 r11 r4       ; Write data to SPI1
read 0 r11 r11       ; Read received data from SPI1
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L126:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_2_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x700000D r11 ; r11 = SPI2 cs register
write 0 r11 r0       ; Set cs low
Label_L127:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_2_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x700000D r11 ; r11 = SPI2 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L128:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_2_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x700000C r11 ; r11 = SPI2 data register
write 0 r11 r4       ; Write data to SPI2
read 0 r11 r11       ; Read received data from SPI2
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L129:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_3_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000010 r11 ; r11 = SPI3 cs register
write 0 r11 r0       ; Set cs low
Label_L130:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_3_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000010 r11 ; r11 = SPI3 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L131:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_3_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x700000F r11 ; r11 = SPI3 data register
write 0 r11 r4       ; Write data to SPI3
read 0 r11 r11       ; Read received data from SPI3
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L132:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_4_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000013 r11 ; r11 = SPI4 cs register
write 0 r11 r0       ; Set cs low
Label_L133:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_4_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000013 r11 ; r11 = SPI4 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L134:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_4_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x7000012 r11 ; r11 = SPI4 data register
write 0 r11 r4       ; Write data to SPI4
read 0 r11 r11       ; Read received data from SPI4
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L135:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_5_select:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000016 r11 ; r11 = SPI5 cs register
write 0 r11 r0       ; Set cs low
Label_L136:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_5_deselect:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
load32 0x7000016 r11 ; r11 = SPI5 cs register
load 1 r12           ; r12 = 1
write 0 r11 r12      ; Set cs high
Label_L137:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_5_transfer:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  write -1 r14 r0
load32 0x7000015 r11 ; r11 = SPI5 data register
write 0 r11 r4       ; Write data to SPI5
read 0 r11 r11       ; Read received data from SPI5
write -1 r14 r11     ; Write received data to stack for return
  read -1 r14 r1
Label_L138:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_transfer:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  write -1 r14 r0
  read 2 r14 r1
  jump Label_L141
Label_L142:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_0_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L143:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_1_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L144:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_2_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L145:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_3_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L146:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_4_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L147:
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_5_transfer
  sub r13 -4 r13
  write -1 r14 r1
  jump Label_L140
Label_L148:
  jump Label_L140
  jump Label_L140
Label_L141:
  load32 0 r12
  beq r1 r12 Label_L142
  load32 1 r12
  beq r1 r12 Label_L143
  load32 2 r12
  beq r1 r12 Label_L144
  load32 3 r12
  beq r1 r12 Label_L145
  load32 4 r12
  beq r1 r12 Label_L146
  load32 5 r12
  beq r1 r12 Label_L147
  jump Label_L148
Label_L140:
  read -1 r14 r1
Label_L139:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_select:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r1
  jump Label_L151
Label_L152:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_0_select
  sub r13 -4 r13
  jump Label_L150
Label_L153:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_1_select
  sub r13 -4 r13
  jump Label_L150
Label_L154:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_2_select
  sub r13 -4 r13
  jump Label_L150
Label_L155:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_3_select
  sub r13 -4 r13
  jump Label_L150
Label_L156:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_4_select
  sub r13 -4 r13
  jump Label_L150
Label_L157:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_5_select
  sub r13 -4 r13
  jump Label_L150
Label_L158:
  jump Label_L150
  jump Label_L150
Label_L151:
  load32 0 r12
  beq r1 r12 Label_L152
  load32 1 r12
  beq r1 r12 Label_L153
  load32 2 r12
  beq r1 r12 Label_L154
  load32 3 r12
  beq r1 r12 Label_L155
  load32 4 r12
  beq r1 r12 Label_L156
  load32 5 r12
  beq r1 r12 Label_L157
  jump Label_L158
Label_L150:
Label_L149:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_deselect:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r1
  jump Label_L161
Label_L162:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_0_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L163:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_1_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L164:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_2_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L165:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_3_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L166:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_4_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L167:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_5_deselect
  sub r13 -4 r13
  jump Label_L160
Label_L168:
  jump Label_L160
  jump Label_L160
Label_L161:
  load32 0 r12
  beq r1 r12 Label_L162
  load32 1 r12
  beq r1 r12 Label_L163
  load32 2 r12
  beq r1 r12 Label_L164
  load32 3 r12
  beq r1 r12 Label_L165
  load32 4 r12
  beq r1 r12 Label_L166
  load32 5 r12
  beq r1 r12 Label_L167
  jump Label_L168
Label_L160:
Label_L159:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_read_jedec_id:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 159 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r1
  push r1
; Stack depth: 1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  pop r11
; Stack depth: 0
  write 0 r11 r1
  read 4 r14 r1
  push r1
; Stack depth: 1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  pop r11
; Stack depth: 0
  write 0 r11 r1
  read 5 r14 r1
  push r1
; Stack depth: 1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  pop r11
; Stack depth: 0
  write 0 r11 r1
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L169:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_enable_write:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 6 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L170:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_disable_write:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 4 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L171:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_read_status:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 5 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r1
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read -1 r14 r1
Label_L172:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_flash_is_busy:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_read_status
  sub r13 -4 r13
  and r1 1 r1
Label_L173:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_wait_busy:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
Label_L175:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_is_busy
  sub r13 -4 r13
  beq r1 r0 Label_L176
  jump Label_L175
Label_L176:
Label_L174:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_write_status:
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
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 1 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L177:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_write_page:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 5 r14 r1
  slt r1 257 r1
  bne r1 r0 Label_L179
  load32 256 r1
  write 5 r14 r1
Label_L179:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 2 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r0
Label_L181:
  read -1 r14 r1
  read 5 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L184
  read 4 r14 r5
  push r5
; Stack depth: 1
  read -1 r14 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  read 0 r5 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
Label_L182:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L181
Label_L184:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L178:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_flash_read_data:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 3 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r0
Label_L186:
  read -1 r14 r1
  read 5 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L189
  read 4 r14 r1
  push r1
; Stack depth: 1
  read -1 r14 r1
  pop r11
; Stack depth: 0
  add r11 r1 r1
  push r1
; Stack depth: 1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  pop r11
; Stack depth: 0
  write 0 r11 r1
Label_L187:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L186
Label_L189:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L185:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_flash_erase_sector:
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
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 32 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L190:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_erase_block_32k:
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
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 82 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L191:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_erase_block_64k:
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
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 216 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L192:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_erase_chip:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 199 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L193:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_spi_flash_read_unique_id:
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
  jump Label_spi_select
  sub r13 -4 r13
  load32 75 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r0
Label_L195:
  read -1 r14 r1
  slt r1 8 r1
  beq r1 r0 Label_L198
  read 3 r14 r1
  push r1
; Stack depth: 1
  read -1 r14 r1
  pop r11
; Stack depth: 0
  add r11 r1 r1
  push r1
; Stack depth: 1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  pop r11
; Stack depth: 0
  write 0 r11 r1
Label_L196:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L195
Label_L198:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L194:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_spi_flash_write_words:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  read 5 r14 r1
  slt r1 65 r1
  bne r1 r0 Label_L200
  load32 64 r1
  write 5 r14 r1
Label_L200:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_enable_write
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 2 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r0
Label_L202:
  read -1 r14 r1
  read 5 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L205
  read 4 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -2 r14 r1
  read -2 r14 r5
  shiftr r5 24 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read -2 r14 r5
  shiftr r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read -2 r14 r5
  shiftr r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read -2 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
Label_L203:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L202
Label_L205:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_wait_busy
  sub r13 -4 r13
Label_L199:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_spi_flash_read_words:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          7 r13
  write          5 r13 r14
  add r13          5 r14
   write 1 r14 r15
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_select
  sub r13 -4 r13
  load32 3 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 16 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  shiftrs r5 8 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  read 3 r14 r5
  and r5 255 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -1 r14 r0
Label_L207:
  read -1 r14 r1
  read 5 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L210
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -2 r14 r1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -3 r14 r1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -4 r14 r1
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_transfer
  sub r13 -4 r13
  write -5 r14 r1
  read 4 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read -2 r14 r8
  shiftl r8 24 r8
  read -3 r14 r9
  shiftl r9 16 r9
  or r8 r9 r8
  read -4 r14 r9
  shiftl r9 8 r9
  or r8 r9 r8
  read -5 r14 r9
  or r8 r9 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L208:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L207
Label_L210:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_deselect
  sub r13 -4 r13
Label_L206:
  read 1 r14 r15
  read 0 r14 r14
  add r13 7 r13
  jumpr 0 r15

.bss
Label_brfs:
  .dw 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0

.code
Label_brfs_compress_string:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
  ;write 1 r14 r15
  write -3 r14 r0
  write -1 r14 r0
  write -2 r14 r0
Label_L212:
  read 3 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  and r1 255 r1
  write -4 r14 r1
  read -4 r14 r1
  load32 24 r8
  read -2 r14 r9
  and r9 3 r9
  shiftl r9 3 r9
  sub r8 r9 r8
  shiftl r1 r8 r1
  read -1 r14 r12
  or r12 r1 r1
  write -1 r14 r1
  read -4 r14 r1
  bne r1 r0 Label_L215
  read 2 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read -1 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
Label_L217:
  read -3 r14 r1
  sltu r1 4 r1
  beq r1 r0 Label_L218
  read 2 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L217
Label_L218:
  jump Label_L213
Label_L215:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  read -2 r14 r1
  and r1 3 r1
  bne r1 r0 Label_L219
  read 2 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read -1 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  write -1 r14 r0
  read -3 r14 r1
  sltu r1 4 r1
  bne r1 r0 Label_L221
  jump Label_L213
Label_L221:
Label_L219:
  jump Label_L212
Label_L213:
Label_L211:
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_decompress_string:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
  ;write 1 r14 r15
  write -2 r14 r0
  write -1 r14 r0
Label_L224:
  read -1 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L227
  read 3 r14 r1
  read -1 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -3 r14 r1
  read -3 r14 r1
  shiftr r1 24 r1
  and r1 255 r1
  write -4 r14 r1
  read 2 r14 r1
  read -2 r14 r8
  add r8 1 r8
  write -2 r14 r8
  add r8 -1 r8
  add r1 r8 r1
  read -4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -4 r14 r1
  bne r1 r0 Label_L228
  jump Label_L223
Label_L228:
  read -3 r14 r1
  shiftr r1 16 r1
  and r1 255 r1
  write -4 r14 r1
  read 2 r14 r1
  read -2 r14 r8
  add r8 1 r8
  write -2 r14 r8
  add r8 -1 r8
  add r1 r8 r1
  read -4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -4 r14 r1
  bne r1 r0 Label_L230
  jump Label_L223
Label_L230:
  read -3 r14 r1
  shiftr r1 8 r1
  and r1 255 r1
  write -4 r14 r1
  read 2 r14 r1
  read -2 r14 r8
  add r8 1 r8
  write -2 r14 r8
  add r8 -1 r8
  add r1 r8 r1
  read -4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -4 r14 r1
  bne r1 r0 Label_L232
  jump Label_L223
Label_L232:
  read -3 r14 r1
  and r1 255 r1
  write -4 r14 r1
  read 2 r14 r1
  read -2 r14 r8
  add r8 1 r8
  write -2 r14 r8
  add r8 -1 r8
  add r1 r8 r1
  read -4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read -4 r14 r1
  bne r1 r0 Label_L234
  jump Label_L223
Label_L234:
Label_L225:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L224
Label_L227:
  read 2 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  write 0 r1 r0
Label_L223:
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_parse_path:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
   write 1 r14 r15
  read 2 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L243
  read 3 r14 r1
  sltu r1 1 r1
Label_L243:
  sltu r0 r1 r1
  bne r1 r0 Label_L242
  read 4 r14 r1
  sltu r1 1 r1
Label_L242:
  beq r1 r0 Label_L237
  load32 -1 r1
  jump Label_L236
Label_L237:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L246
  read -1 r14 r1
  slt r1 128 r1
  xor r1 1 r1
Label_L246:
  beq r1 r0 Label_L244
  load32 -12 r1
  jump Label_L236
Label_L244:
  load32 -1 r1
  write -3 r14 r1
  read -1 r14 r1
  sub r1 1 r1
  write -2 r14 r1
Label_L247:
  read -2 r14 r1
  blts r1 r0 Label_L250
  read 2 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  bne r1 r0 Label_L251
  read -2 r14 r1
  write -3 r14 r1
  jump Label_L250
Label_L251:
Label_L248:
  read -2 r14 r1
  sub r1 1 r1
  write -2 r14 r1
  sub r1 -1 r1
  jump Label_L247
Label_L250:
  read -3 r14 r1
  bges r1 r0 Label_L253
  read 3 r14 r1
  add r1 0 r1
  load32 47 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r1
  add r1 1 r1
  write 0 r1 r0
  read 2 r14 r5
  read 4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcpy
  sub r13 -4 r13
  jump Label_L254
Label_L253:
  read -3 r14 r1
  bne r1 r0 Label_L255
  read 3 r14 r1
  add r1 0 r1
  load32 47 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  read 3 r14 r1
  add r1 1 r1
  write 0 r1 r0
  read 2 r14 r5
  add r5 1 r5
  read 4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcpy
  sub r13 -4 r13
  jump Label_L256
Label_L255:
  read -3 r14 r1
  read 5 r14 r8
  sltu r1 r8 r1
  bne r1 r0 Label_L257
  load32 -12 r1
  jump Label_L236
Label_L257:
  write -2 r14 r0
Label_L260:
  read -2 r14 r1
  read -3 r14 r8
  slt r1 r8 r1
  beq r1 r0 Label_L263
  read 3 r14 r1
  read -2 r14 r8
  add r1 r8 r1
  read 2 r14 r8
  read -2 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
Label_L261:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L260
Label_L263:
  read 3 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read 2 r14 r5
  push r5
; Stack depth: 1
  read -3 r14 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  add r5 1 r5
  read 4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcpy
  sub r13 -4 r13
Label_L256:
Label_L254:
  read 4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  sltu r1 1 r1
  bne r1 r0 Label_L266
  read 4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  sltu r1 17 r1
  xor r1 1 r1
Label_L266:
  beq r1 r0 Label_L264
  load32 -13 r1
  jump Label_L236
Label_L264:
  load32 0 r1
Label_L236:
  read 1 r14 r15
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_brfs_get_superblock:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 0 r1
  read 0 r1 r1
Label_L267:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_get_fat:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 0 r1
  read 0 r1 r1
  add r1 16 r1
Label_L268:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_get_data_block:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  addr2reg Label_brfs r1
  add r1 0 r1
  read 0 r1 r1
  add r1 16 r1
  read -1 r14 r8
  add r8 0 r8
  read 0 r8 r8
  add r1 r8 r1
  read 2 r14 r8
  read -1 r14 r9
  add r9 1 r9
  read 0 r9 r9
  mults r8 r9 r8
  add r1 r8 r1
Label_L269:
  read 1 r14 r15
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_brfs_find_free_block:
  sub r13          5 r13
  write          3 r13 r14
  add r13          3 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  write -3 r14 r0
Label_L273:
  read -3 r14 r1
  read -1 r14 r8
  add r8 0 r8
  read 0 r8 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L276
  read -2 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  bne r1 r0 Label_L277
  read -3 r14 r1
  jump Label_L271
Label_L277:
Label_L274:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L273
Label_L276:
  load32 -4 r1
Label_L271:
  read 1 r14 r15
  read 0 r14 r14
  add r13 5 r13
  jumpr 0 r15

.code
Label_brfs_find_free_dir_entry:
  write 0 r13 r4
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -2 r14 r1
  write -3 r14 r0
Label_L282:
  read -3 r14 r1
  read -2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L285
  read 2 r14 r1
  read -3 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -4 r14 r1
  read -4 r14 r1
  add r1 0 r1
  read 0 r1 r1
  bne r1 r0 Label_L287
  read -3 r14 r1
  jump Label_L280
Label_L287:
Label_L283:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L282
Label_L285:
  load32 -5 r1
Label_L280:
  read 1 r14 r15
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_mark_block_dirty:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 55 r1
  read 2 r14 r8
  shiftr r8 5 r8
  add r1 r8 r1
  load32 1 r8
  read 2 r14 r9
  and r9 31 r9
  shiftl r8 r9 r8
  or r1 r0 r12
  read 0 r1 r1
  or r1 r8 r1
  write 0 r12 r1
Label_L290:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_is_block_dirty:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 55 r1
  read 2 r14 r8
  shiftr r8 5 r8
  add r1 r8 r1
  read 0 r1 r1
  read 2 r14 r8
  and r8 31 r8
  shiftr r1 r8 r1
  and r1 1 r1
Label_L291:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_get_fat_idx_at_offset:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  read 2 r14 r1
  write -3 r14 r1
  read 3 r14 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  divu r1 r8 r1
  write -4 r14 r1
Label_L294:
  read -4 r14 r1
  beq r1 r0 Label_L295
  read -2 r14 r1
  read -3 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -3 r14 r1
  read -3 r14 r1
  xor r1 -1 r1
  bne r1 r0 Label_L296
  load32 -16 r1
  jump Label_L292
Label_L296:
  read -4 r14 r1
  sub r1 1 r1
  write -4 r14 r1
  sub r1 -1 r1
  jump Label_L294
Label_L295:
  read -3 r14 r1
Label_L292:
  read 1 r14 r15
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_get_dir_fat_idx:
  write 0 r13 r4
  sub r13        154 r13
  write        152 r13 r14
  add r13        152 r14
   write 1 r14 r15
  read 2 r14 r1
  bne r1 r0 Label_L301
  load32 -1 r1
  jump Label_L300
Label_L301:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  write -150 r14 r1
  read -150 r14 r1
  slt r1 128 r1
  bne r1 r0 Label_L304
  load32 -12 r1
  jump Label_L300
Label_L304:
  read -150 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L308
  read -150 r14 r1
  xor r1 1 r1
  sltu r1 1 r1
  beq r1 r0 Label_L309
  read 2 r14 r1
  add r1 0 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  sltu r1 1 r1
Label_L309:
  sltu r0 r1 r1
Label_L308:
  beq r1 r0 Label_L306
  load32 0 r1
  jump Label_L300
Label_L306:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  write -2 r14 r0
  read 2 r14 r5
  add r14 -130 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcpy
  sub r13 -4 r13
  write -148 r14 r0
  read -130 r14 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  bne r1 r0 Label_L311
  load32 1 r1
  write -148 r14 r1
Label_L311:
Label_L313:
  add r14 -130 r1
  read -148 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  beq r1 r0 Label_L314
  write -149 r14 r0
Label_L315:
  add r14 -130 r1
  read -148 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
  beq r1 r0 Label_L317
  add r14 -130 r1
  read -148 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  sltu r0 r1 r1
Label_L317:
  beq r1 r0 Label_L316
  read -149 r14 r1
  slt r1 16 r1
  bne r1 r0 Label_L318
  load32 -13 r1
  jump Label_L300
Label_L318:
  add r14 -147 r1
  read -149 r14 r8
  add r8 1 r8
  write -149 r14 r8
  add r8 -1 r8
  add r1 r8 r1
  add r14 -130 r8
  read -148 r14 r9
  add r9 1 r9
  write -148 r14 r9
  add r9 -1 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  jump Label_L315
Label_L316:
  add r14 -147 r1
  read -149 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  add r14 -130 r1
  read -148 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  bne r1 r0 Label_L320
  read -148 r14 r1
  add r1 1 r1
  write -148 r14 r1
  add r1 -1 r1
Label_L320:
  read -149 r14 r1
  bne r1 r0 Label_L322
  jump Label_L313
Label_L322:
  add r14 -151 r6
  add r14 -147 r5
  read -2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -152 r14 r1
  read -152 r14 r1
  beq r1 r0 Label_L324
  read -152 r14 r1
  jump Label_L300
Label_L324:
  read -151 r14 r1
  add r1 5 r1
  read 0 r1 r1
  and r1 1 r1
  bne r1 r0 Label_L326
  load32 -11 r1
  jump Label_L300
Label_L326:
  read -151 r14 r1
  add r1 6 r1
  read 0 r1 r1
  write -2 r14 r1
  jump Label_L313
Label_L314:
  read -2 r14 r1
Label_L300:
  read 1 r14 r15
  read 0 r14 r14
  add r13 154 r13
  jumpr 0 r15

.code
Label_brfs_find_in_directory:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13         24 r13
  write         22 r13 r14
  add r13         22 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -2 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -3 r14 r1
  write -4 r14 r0
Label_L331:
  read -4 r14 r1
  read -3 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L334
  read -2 r14 r1
  read -4 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -5 r14 r1
  read -5 r14 r1
  add r1 0 r1
  read 0 r1 r1
  beq r1 r0 Label_L336
  load32 4 r6
  read -5 r14 r5
  add r5 0 r5
  add r14 -22 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_decompress_string
  sub r13 -4 r13
  read 3 r14 r5
  add r14 -22 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  bne r1 r0 Label_L338
  read 4 r14 r1
  beq r1 r0 Label_L340
  read 4 r14 r1
  read -5 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L340:
  load32 0 r1
  jump Label_L329
Label_L338:
Label_L336:
Label_L332:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L331
Label_L334:
  load32 -2 r1
Label_L329:
  read 1 r14 r15
  read 0 r14 r14
  add r13 24 r13
  jumpr 0 r15

.code
Label_brfs_create_dir_entry:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  pop r11
  write 4 r13 r11
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
   write 1 r14 r15
  load32 8 r6
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read 3 r14 r5
  read 2 r14 r4
  add r4 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_compress_string
  sub r13 -4 r13
  read 2 r14 r1
  add r1 6 r1
  read 4 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read 2 r14 r1
  add r1 7 r1
  read 5 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read 2 r14 r1
  add r1 5 r1
  read 6 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read 2 r14 r1
  add r1 4 r1
  write 0 r1 r0
Label_L343:
  read 1 r14 r15
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_init_directory_block:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13         12 r13
  write         10 r13 r14
  add r13         10 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -10 r14 r1
  read -1 r14 r6
  add r6 1 r6
  read 0 r6 r6
  load32 0 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13

.rdata
Label_L347:
  .dsw "."
  .dw 0

.code
  load32 1 r1
  push r1
; Stack depth: 1
  read -10 r14 r7
  shiftl r7 3 r7
  read 3 r14 r6
  addr2reg Label_L347 r5
  add r14 -9 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_create_dir_entry
  sub r13 -5 r13
  load32 8 r6
  add r14 -9 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13

.rdata
Label_L348:
  .dsw ".."
  .dw 0

.code
  load32 1 r1
  push r1
; Stack depth: 1
  read -10 r14 r7
  shiftl r7 3 r7
  read 4 r14 r6
  addr2reg Label_L348 r5
  add r14 -9 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_create_dir_entry
  sub r13 -5 r13
  load32 8 r6
  add r14 -9 r5
  read 2 r14 r4
  add r4 8 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
Label_L345:
  read 1 r14 r15
  read 0 r14 r14
  add r13 12 r13
  jumpr 0 r15

.code
Label_brfs_init:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 0 r1
  load32 8388608 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_brfs r1
  add r1 1 r1
  load32 8388608 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_brfs r1
  add r1 2 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 3 r1
  read 2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_brfs r1
  add r1 4 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 5 r1
  load32 4096 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_brfs r1
  add r1 6 r1
  load32 65536 r8
  write 0 r1 r8
  or r8 r0 r1
  write -1 r14 r0
Label_L351:
  read -1 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L354
  addr2reg Label_brfs r1
  add r1 7 r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -1 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L352:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L351
Label_L354:
  write -1 r14 r0
Label_L356:
  read -1 r14 r1
  sltu r1 2048 r1
  beq r1 r0 Label_L359
  addr2reg Label_brfs r1
  add r1 55 r1
  read -1 r14 r8
  add r1 r8 r1
  write 0 r1 r0
Label_L357:
  read -1 r14 r1
  add r1 1 r1
  write -1 r14 r1
  add r1 -1 r1
  jump Label_L356
Label_L359:
  load32 0 r1
Label_L349:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_brfs_format:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  write 3 r13 r7
  sub r13          7 r13
  write          5 r13 r14
  add r13          5 r14
   write 1 r14 r15
  read 2 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L363
  read 2 r14 r1
  push r12
  load32 65537 r12
  sltu r1 r12 r1
  pop r12
  xor r1 1 r1
Label_L363:
  beq r1 r0 Label_L361
  load32 -1 r1
  jump Label_L360
Label_L361:
  read 3 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L366
  read 3 r14 r1
  sltu r1 2049 r1
  xor r1 1 r1
Label_L366:
  beq r1 r0 Label_L364
  load32 -1 r1
  jump Label_L360
Label_L364:
  read 2 r14 r1
  and r1 63 r1
  beq r1 r0 Label_L367
  load32 -1 r1
  jump Label_L360
Label_L367:
  read 3 r14 r1
  and r1 63 r1
  beq r1 r0 Label_L369
  load32 -1 r1
  jump Label_L360
Label_L369:
  read 2 r14 r1
  add r1 16 r1
  read 2 r14 r8
  read 3 r14 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -5 r14 r1
  read -5 r14 r1
  addr2reg Label_brfs r8
  add r8 1 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L371
  load32 -4 r1
  jump Label_L360
Label_L371:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  load32 16 r6
  load32 0 r5
  read -1 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read -1 r14 r1
  add r1 0 r1
  read 2 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -1 r14 r1
  add r1 1 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -1 r14 r1
  add r1 12 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  read 4 r14 r1
  beq r1 r0 Label_L375
  write -4 r14 r0
Label_L378:
  read -4 r14 r1
  sltu r1 10 r1
  beq r1 r0 Label_L382
  read 4 r14 r1
  read -4 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  sltu r0 r1 r1
Label_L382:
  beq r1 r0 Label_L381
  read -1 r14 r1
  add r1 2 r1
  read -4 r14 r8
  add r1 r8 r1
  read 4 r14 r8
  read -4 r14 r9
  add r8 r9 r8
  read 0 r8 r8
  shiftl r8 24 r8
  shiftrs r8 24 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L379:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L378
Label_L381:
Label_L375:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  read 2 r14 r6
  load32 0 r5
  read -2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read 5 r14 r1
  beq r1 r0 Label_L384
  load32 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  read 2 r14 r6
  push r6
; Stack depth: 1
  read 3 r14 r6
  pop r11
; Stack depth: 0
  mults r11 r6 r6
  load32 0 r5
  read -3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
Label_L384:
  load32 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  load32 0 r6
  load32 0 r5
  read -3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_init_directory_block
  sub r13 -4 r13
  read -2 r14 r1
  add r1 0 r1
  load32 -1 r8
  write 0 r1 r8
  or r8 r0 r1
  write -4 r14 r0
Label_L387:
  read -4 r14 r1
  read 2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L390
  read -4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
Label_L388:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L387
Label_L390:
  addr2reg Label_brfs r5
  add r5 4 r5
  read 0 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_erase_sector
  sub r13 -4 r13
  load32 16 r7
  read -1 r14 r6
  addr2reg Label_brfs r5
  add r5 4 r5
  read 0 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_write_words
  sub r13 -4 r13
  addr2reg Label_brfs r1
  add r1 2 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  load32 0 r1
Label_L360:
  read 1 r14 r15
  read 0 r14 r14
  add r13 7 r13
  jumpr 0 r15

.code
Label_brfs_validate_superblock:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  add r1 12 r1
  read 0 r1 r1
  xor r1 1 r1
  beq r1 r0 Label_L393
  load32 -14 r1
  jump Label_L392
Label_L393:
  read 2 r14 r1
  add r1 0 r1
  read 0 r1 r1
  sltu r1 1 r1
  bne r1 r0 Label_L397
  read 2 r14 r1
  add r1 0 r1
  read 0 r1 r1
  push r12
  load32 65537 r12
  sltu r1 r12 r1
  pop r12
  xor r1 1 r1
Label_L397:
  beq r1 r0 Label_L395
  load32 -14 r1
  jump Label_L392
Label_L395:
  read 2 r14 r1
  add r1 0 r1
  read 0 r1 r1
  and r1 63 r1
  beq r1 r0 Label_L398
  load32 -14 r1
  jump Label_L392
Label_L398:
  read 2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  sltu r1 1 r1
  bne r1 r0 Label_L402
  read 2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  sltu r1 2049 r1
  xor r1 1 r1
Label_L402:
  beq r1 r0 Label_L400
  load32 -14 r1
  jump Label_L392
Label_L400:
  load32 0 r1
Label_L392:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_mount:
  sub r13          8 r13
  write          6 r13 r14
  add r13          6 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  load32 16 r7
  read -1 r14 r6
  addr2reg Label_brfs r5
  add r5 4 r5
  read 0 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_read_words
  sub r13 -4 r13
  read -1 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_validate_superblock
  sub r13 -4 r13
  write -6 r14 r1
  read -6 r14 r1
  beq r1 r0 Label_L406
  read -6 r14 r1
  jump Label_L403
Label_L406:
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  add r1 16 r1
  read -1 r14 r8
  add r8 0 r8
  read 0 r8 r8
  read -1 r14 r9
  add r9 1 r9
  read 0 r9 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -4 r14 r1
  read -4 r14 r1
  addr2reg Label_brfs r8
  add r8 1 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L408
  load32 -4 r1
  jump Label_L403
Label_L408:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  read -1 r14 r7
  add r7 0 r7
  read 0 r7 r7
  read -2 r14 r6
  addr2reg Label_brfs r5
  add r5 5 r5
  read 0 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_read_words
  sub r13 -4 r13
  load32 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  mults r1 r8 r1
  write -4 r14 r1
  read -4 r14 r7
  read -3 r14 r6
  addr2reg Label_brfs r5
  add r5 6 r5
  read 0 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_read_words
  sub r13 -4 r13
  write -5 r14 r0
Label_L410:
  read -5 r14 r1
  sltu r1 2048 r1
  beq r1 r0 Label_L413
  addr2reg Label_brfs r1
  add r1 55 r1
  read -5 r14 r8
  add r1 r8 r1
  write 0 r1 r0
Label_L411:
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
  jump Label_L410
Label_L413:
  write -5 r14 r0
Label_L414:
  read -5 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L417
  addr2reg Label_brfs r1
  add r1 7 r1
  read -5 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -5 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -5 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L415:
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
  jump Label_L414
Label_L417:
  addr2reg Label_brfs r1
  add r1 2 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  load32 0 r1
Label_L403:
  read 1 r14 r15
  read 0 r14 r14
  add r13 8 r13
  jumpr 0 r15

.code
Label_brfs_unmount:
  sub r13          4 r13
  write          2 r13 r14
  add r13          2 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L420
  load32 -19 r1
  jump Label_L419
Label_L420:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_sync
  sub r13 -4 r13
  write -1 r14 r1
  read -1 r14 r1
  beq r1 r0 Label_L422
  read -1 r14 r1
  jump Label_L419
Label_L422:
  write -2 r14 r0
Label_L424:
  read -2 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L427
  addr2reg Label_brfs r1
  add r1 7 r1
  read -2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
Label_L425:
  read -2 r14 r1
  add r1 1 r1
  write -2 r14 r1
  add r1 -1 r1
  jump Label_L424
Label_L427:
  addr2reg Label_brfs r1
  add r1 2 r1
  write 0 r1 r0
  load32 0 r1
Label_L419:
  read 1 r14 r15
  read 0 r14 r14
  add r13 4 r13
  jumpr 0 r15

.code
Label_brfs_write_fat_sector:
  write 0 r13 r4
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  addr2reg Label_brfs r1
  add r1 5 r1
  read 0 r1 r1
  read 2 r14 r8
  shiftl r8 12 r8
  add r1 r8 r1
  write -1 r14 r1
  read 2 r14 r1
  shiftl r1 10 r1
  write -3 r14 r1
  read -1 r14 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_erase_sector
  sub r13 -4 r13
  write -4 r14 r0
Label_L430:
  read -4 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L433
  load32 64 r7
  read -2 r14 r6
  push r6
; Stack depth: 1
  read -3 r14 r6
  pop r11
; Stack depth: 0
  add r11 r6 r6
  push r6
; Stack depth: 1
  read -4 r14 r6
  shiftl r6 6 r6
  pop r11
; Stack depth: 0
  add r11 r6 r6
  read -1 r14 r5
  push r5
; Stack depth: 1
  read -4 r14 r5
  shiftl r5 8 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_write_words
  sub r13 -4 r13
Label_L431:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L430
Label_L433:
Label_L429:
  read 1 r14 r15
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_write_data_sector:
  write 0 r13 r4
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
   write 1 r14 r15
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  load32 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  addr2reg Label_brfs r1
  add r1 6 r1
  read 0 r1 r1
  read 2 r14 r8
  shiftl r8 12 r8
  add r1 r8 r1
  write -2 r14 r1
  read -2 r14 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_erase_sector
  sub r13 -4 r13
  write -4 r14 r0
Label_L436:
  read -4 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L439
  load32 64 r7
  read -3 r14 r6
  push r6
; Stack depth: 1
  read 2 r14 r6
  shiftl r6 10 r6
  pop r11
; Stack depth: 0
  add r11 r6 r6
  push r6
; Stack depth: 1
  read -4 r14 r6
  shiftl r6 6 r6
  pop r11
; Stack depth: 0
  add r11 r6 r6
  read -2 r14 r5
  push r5
; Stack depth: 1
  read -4 r14 r5
  shiftl r5 8 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  addr2reg Label_brfs r4
  add r4 3 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_spi_flash_write_words
  sub r13 -4 r13
Label_L437:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L436
Label_L439:
Label_L434:
  read 1 r14 r15
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_sync:
  sub r13         10 r13
  write          8 r13 r14
  add r13          8 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L441
  load32 -19 r1
  jump Label_L440
Label_L441:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  load32 1024 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  divu r1 r8 r1
  write -2 r14 r1
  read -2 r14 r1
  bne r1 r0 Label_L444
  load32 1 r1
  write -2 r14 r1
Label_L444:
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  add r1 1024 r1
  sub r1 1 r1
  shiftr r1 10 r1
  write -6 r14 r1
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  mults r1 r8 r1
  add r1 1024 r1
  sub r1 1 r1
  shiftr r1 10 r1
  write -7 r14 r1
  write -3 r14 r0
Label_L446:
  read -3 r14 r1
  read -6 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L449
  write -8 r14 r0
  write -5 r14 r0
Label_L450:
  read -5 r14 r1
  sltu r1 1024 r1
  beq r1 r0 Label_L454
  read -8 r14 r1
  sltu r1 1 r1
Label_L454:
  beq r1 r0 Label_L453
  read -3 r14 r1
  shiftl r1 10 r1
  read -5 r14 r8
  add r1 r8 r1
  write -4 r14 r1
  read -4 r14 r1
  push r1
; Stack depth: 1
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  pop r11
; Stack depth: 0
  sltu r11 r1 r1
  beq r1 r0 Label_L457
  read -4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_is_block_dirty
  sub r13 -4 r13
  sltu r0 r1 r1
Label_L457:
  beq r1 r0 Label_L455
  load32 1 r1
  write -8 r14 r1
Label_L455:
Label_L451:
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
  jump Label_L450
Label_L453:
  read -8 r14 r1
  beq r1 r0 Label_L458
  read -3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_write_fat_sector
  sub r13 -4 r13
Label_L458:
Label_L447:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L446
Label_L449:
  write -3 r14 r0
Label_L460:
  read -3 r14 r1
  read -7 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L463
  write -8 r14 r0
  write -5 r14 r0
Label_L464:
  read -5 r14 r1
  read -2 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L468
  read -8 r14 r1
  sltu r1 1 r1
Label_L468:
  beq r1 r0 Label_L467
  read -3 r14 r1
  read -2 r14 r8
  mults r1 r8 r1
  read -5 r14 r8
  add r1 r8 r1
  write -4 r14 r1
  read -4 r14 r1
  push r1
; Stack depth: 1
  read -1 r14 r1
  add r1 0 r1
  read 0 r1 r1
  pop r11
; Stack depth: 0
  sltu r11 r1 r1
  beq r1 r0 Label_L471
  read -4 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_is_block_dirty
  sub r13 -4 r13
  sltu r0 r1 r1
Label_L471:
  beq r1 r0 Label_L469
  load32 1 r1
  write -8 r14 r1
Label_L469:
Label_L465:
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
  jump Label_L464
Label_L467:
  read -8 r14 r1
  beq r1 r0 Label_L472
  read -3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_write_data_sector
  sub r13 -4 r13
Label_L472:
Label_L461:
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
  jump Label_L460
Label_L463:
  write -5 r14 r0
Label_L474:
  read -5 r14 r1
  sltu r1 2048 r1
  beq r1 r0 Label_L477
  addr2reg Label_brfs r1
  add r1 55 r1
  read -5 r14 r8
  add r1 r8 r1
  write 0 r1 r0
Label_L475:
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
  jump Label_L474
Label_L477:
  load32 0 r1
Label_L440:
  read 1 r14 r15
  read 0 r14 r14
  add r13 10 r13
  jumpr 0 r15

.code
Label_brfs_create_file:
  write 0 r13 r4
  sub r13        163 r13
  write        161 r13 r14
  add r13        161 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L479
  load32 -19 r1
  jump Label_L478
Label_L479:
  load32 128 r7
  add r14 -145 r6
  add r14 -128 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L481
  read -146 r14 r1
  jump Label_L478
Label_L481:
  add r14 -128 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -147 r14 r1
  read -147 r14 r1
  bges r1 r0 Label_L483
  read -147 r14 r1
  jump Label_L478
Label_L483:
  add r14 -152 r6
  add r14 -145 r5
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  bne r1 r0 Label_L485
  load32 -3 r1
  jump Label_L478
Label_L485:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_free_block
  sub r13 -4 r13
  write -148 r14 r1
  read -148 r14 r1
  bges r1 r0 Label_L487
  read -148 r14 r1
  jump Label_L478
Label_L487:
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -150 r14 r1
  read -150 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_free_dir_entry
  sub r13 -4 r13
  write -149 r14 r1
  read -149 r14 r1
  bges r1 r0 Label_L489
  read -149 r14 r1
  jump Label_L478
Label_L489:
  load32 0 r1
  push r1
; Stack depth: 1
  load32 0 r7
  read -148 r14 r6
  add r14 -145 r5
  add r14 -160 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_create_dir_entry
  sub r13 -5 r13
  read -150 r14 r1
  read -149 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -152 r14 r1
  load32 8 r6
  add r14 -160 r5
  read -152 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -151 r14 r1
  read -151 r14 r1
  read -148 r14 r8
  add r1 r8 r1
  load32 -1 r8
  write 0 r1 r8
  or r8 r0 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -161 r14 r1
  read -161 r14 r1
  add r1 1 r1
  read 0 r1 r1
  push r1
; Stack depth: 1
  load32 0 r1
  push r1
; Stack depth: 2
  read -148 r14 r1
  push r1
; Stack depth: 3
  pop r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  push r1
; Stack depth: 4
  pop r4
  pop r5
  pop r6
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  read -148 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  load32 0 r1
Label_L478:
  read 1 r14 r15
  read 0 r14 r14
  add r13 163 r13
  jumpr 0 r15

.code
Label_brfs_create_dir:
  write 0 r13 r4
  sub r13        165 r13
  write        163 r13 r14
  add r13        163 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L496
  load32 -19 r1
  jump Label_L495
Label_L496:
  load32 128 r7
  add r14 -145 r6
  add r14 -128 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L498
  read -146 r14 r1
  jump Label_L495
Label_L498:
  add r14 -128 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -147 r14 r1
  read -147 r14 r1
  bges r1 r0 Label_L500
  read -147 r14 r1
  jump Label_L495
Label_L500:
  add r14 -153 r6
  add r14 -145 r5
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  bne r1 r0 Label_L502
  load32 -3 r1
  jump Label_L495
Label_L502:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_free_block
  sub r13 -4 r13
  write -148 r14 r1
  read -148 r14 r1
  bges r1 r0 Label_L504
  read -148 r14 r1
  jump Label_L495
Label_L504:
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -150 r14 r1
  read -150 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_free_dir_entry
  sub r13 -4 r13
  write -149 r14 r1
  read -149 r14 r1
  bges r1 r0 Label_L506
  read -149 r14 r1
  jump Label_L495
Label_L506:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -162 r14 r1
  read -162 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -163 r14 r1
  load32 1 r1
  push r1
; Stack depth: 1
  read -163 r14 r7
  shiftl r7 3 r7
  read -148 r14 r6
  add r14 -145 r5
  add r14 -161 r4
  sub r13 5 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_create_dir_entry
  sub r13 -5 r13
  read -150 r14 r1
  read -149 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -153 r14 r1
  load32 8 r6
  add r14 -161 r5
  read -153 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  read -148 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -151 r14 r1
  read -147 r14 r6
  read -148 r14 r5
  read -151 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_init_directory_block
  sub r13 -4 r13
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -152 r14 r1
  read -152 r14 r1
  read -148 r14 r8
  add r1 r8 r1
  load32 -1 r8
  write 0 r1 r8
  or r8 r0 r1
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  read -148 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  load32 0 r1
Label_L495:
  read 1 r14 r15
  read 0 r14 r14
  add r13 165 r13
  jumpr 0 r15

.code
Label_brfs_open:
  write 0 r13 r4
  sub r13        152 r13
  write        150 r13 r14
  add r13        150 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L513
  load32 -19 r1
  jump Label_L512
Label_L513:
  load32 128 r7
  add r14 -145 r6
  add r14 -128 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L515
  read -146 r14 r1
  jump Label_L512
Label_L515:
  add r14 -128 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -147 r14 r1
  read -147 r14 r1
  bges r1 r0 Label_L517
  read -147 r14 r1
  jump Label_L512
Label_L517:
  add r14 -148 r6
  add r14 -145 r5
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L519
  read -146 r14 r1
  jump Label_L512
Label_L519:
  read -148 r14 r1
  add r1 5 r1
  read 0 r1 r1
  and r1 1 r1
  beq r1 r0 Label_L521
  load32 -10 r1
  jump Label_L512
Label_L521:
  write -150 r14 r0
Label_L523:
  read -150 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L526
  addr2reg Label_brfs r1
  add r1 7 r1
  read -150 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  read 0 r1 r1
  read -148 r14 r8
  add r8 6 r8
  read 0 r8 r8
  xor r1 r8 r1
  sltu r1 1 r1
  beq r1 r0 Label_L530
  addr2reg Label_brfs r1
  add r1 7 r1
  read -150 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
  sltu r0 r1 r1
Label_L530:
  beq r1 r0 Label_L527
  load32 -7 r1
  jump Label_L512
Label_L527:
Label_L524:
  read -150 r14 r1
  add r1 1 r1
  write -150 r14 r1
  add r1 -1 r1
  jump Label_L523
Label_L526:
  load32 -1 r1
  write -149 r14 r1
  write -150 r14 r0
Label_L531:
  read -150 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L534
  addr2reg Label_brfs r1
  add r1 7 r1
  read -150 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L535
  read -150 r14 r1
  write -149 r14 r1
  jump Label_L534
Label_L535:
Label_L532:
  read -150 r14 r1
  add r1 1 r1
  write -150 r14 r1
  add r1 -1 r1
  jump Label_L531
Label_L534:
  read -149 r14 r1
  bges r1 r0 Label_L539
  load32 -9 r1
  jump Label_L512
Label_L539:
  addr2reg Label_brfs r1
  add r1 7 r1
  read -149 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  read -148 r14 r8
  add r8 6 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
  addr2reg Label_brfs r1
  add r1 7 r1
  read -149 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read -149 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read -148 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -149 r14 r1
Label_L512:
  read 1 r14 r15
  read 0 r14 r14
  add r13 152 r13
  jumpr 0 r15

.code
Label_brfs_close:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L542
  load32 -19 r1
  jump Label_L541
Label_L542:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L546
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L546:
  beq r1 r0 Label_L544
  load32 -1 r1
  jump Label_L541
Label_L544:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L547
  load32 -8 r1
  jump Label_L541
Label_L547:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 0 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 1 r1
  write 0 r1 r0
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  write 0 r1 r0
  load32 0 r1
Label_L541:
  read 0 r14 r14
  add r13 2 r13
  jumpr 0 r15

.code
Label_brfs_read:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13         12 r13
  write         10 r13 r14
  add r13         10 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L552
  load32 -19 r1
  jump Label_L551
Label_L552:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L556
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L556:
  beq r1 r0 Label_L554
  load32 -1 r1
  jump Label_L551
Label_L554:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -2 r14 r1
  read -2 r14 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L557
  load32 -8 r1
  jump Label_L551
Label_L557:
  read 3 r14 r1
  bne r1 r0 Label_L560
  load32 -1 r1
  jump Label_L551
Label_L560:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -4 r14 r1
  read -2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -2 r14 r8
  add r8 2 r8
  read 0 r8 r8
  add r8 7 r8
  read 0 r8 r8
  sltu r1 r8 r1
  bne r1 r0 Label_L564
  load32 0 r1
  jump Label_L551
Label_L564:
  read -2 r14 r1
  add r1 2 r1
  read 0 r1 r1
  add r1 7 r1
  read 0 r1 r1
  read -2 r14 r8
  add r8 1 r8
  read 0 r8 r8
  sub r1 r8 r1
  write -10 r14 r1
  read 4 r14 r1
  read -10 r14 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L566
  read -10 r14 r1
  write 4 r14 r1
Label_L566:
  read -2 r14 r5
  add r5 1 r5
  read 0 r5 r5
  read -2 r14 r4
  add r4 0 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat_idx_at_offset
  sub r13 -4 r13
  write -5 r14 r1
  read -5 r14 r1
  bges r1 r0 Label_L568
  load32 -17 r1
  jump Label_L551
Label_L568:
  write -9 r14 r0
Label_L571:
  read 4 r14 r1
  beq r1 r0 Label_L572
  read -2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  modu r1 r8 r1
  write -6 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -6 r14 r8
  sub r1 r8 r1
  write -8 r14 r1
  read -8 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  bne r1 r0 Label_L573
  read 4 r14 r1
  jump Label_L574
Label_L573:
  read -8 r14 r1
Label_L574:
  write -7 r14 r1
  read -5 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  read -7 r14 r6
  read -3 r14 r5
  push r5
; Stack depth: 1
  read -6 r14 r5
  pop r11
; Stack depth: 0
  add r11 r5 r5
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  read -7 r14 r1
  read 3 r14 r12
  add r12 r1 r1
  write 3 r14 r1
  read -2 r14 r1
  add r1 1 r1
  read -7 r14 r8
  or r1 r0 r12
  read 0 r1 r1
  add r1 r8 r1
  write 0 r12 r1
  read -7 r14 r1
  read -9 r14 r12
  add r12 r1 r1
  write -9 r14 r1
  read -7 r14 r1
  read 4 r14 r12
  sub r12 r1 r1
  write 4 r14 r1
  read 4 r14 r1
  beq r1 r0 Label_L575
  read -4 r14 r1
  read -5 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -5 r14 r1
  read -5 r14 r1
  xor r1 -1 r1
  bne r1 r0 Label_L577
  jump Label_L572
Label_L577:
Label_L575:
  jump Label_L571
Label_L572:
  read -9 r14 r1
Label_L551:
  read 1 r14 r15
  read 0 r14 r14
  add r13 12 r13
  jumpr 0 r15

.code
Label_brfs_write:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13         13 r13
  write         11 r13 r14
  add r13         11 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L582
  load32 -19 r1
  jump Label_L581
Label_L582:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L586
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L586:
  beq r1 r0 Label_L584
  load32 -1 r1
  jump Label_L581
Label_L584:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -2 r14 r1
  read -2 r14 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L587
  load32 -8 r1
  jump Label_L581
Label_L587:
  read 3 r14 r1
  sltu r1 1 r1
  beq r1 r0 Label_L593
  read 4 r14 r1
  sltu r0 r1 r1
Label_L593:
  beq r1 r0 Label_L590
  load32 -1 r1
  jump Label_L581
Label_L590:
  read 4 r14 r1
  bne r1 r0 Label_L594
  load32 0 r1
  jump Label_L581
Label_L594:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -4 r14 r1
  read -2 r14 r5
  add r5 1 r5
  read 0 r5 r5
  read -2 r14 r4
  add r4 0 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat_idx_at_offset
  sub r13 -4 r13
  write -11 r14 r1
  read -11 r14 r1
  bges r1 r0 Label_L597
  load32 -16 r1
  jump Label_L581
Label_L597:
  read -11 r14 r1
  write -5 r14 r1
  write -9 r14 r0
Label_L600:
  read 4 r14 r1
  beq r1 r0 Label_L601
  read -2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  modu r1 r8 r1
  write -6 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -6 r14 r8
  sub r1 r8 r1
  write -8 r14 r1
  read -8 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
  bne r1 r0 Label_L602
  read 4 r14 r1
  jump Label_L603
Label_L602:
  read -8 r14 r1
Label_L603:
  write -7 r14 r1
  read -5 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  read -7 r14 r6
  read 3 r14 r5
  read -3 r14 r4
  push r4
; Stack depth: 1
  read -6 r14 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  read -5 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  read -7 r14 r1
  read 3 r14 r12
  add r12 r1 r1
  write 3 r14 r1
  read -2 r14 r1
  add r1 1 r1
  read -7 r14 r8
  or r1 r0 r12
  read 0 r1 r1
  add r1 r8 r1
  write 0 r12 r1
  read -7 r14 r1
  read -9 r14 r12
  add r12 r1 r1
  write -9 r14 r1
  read -7 r14 r1
  read 4 r14 r12
  sub r12 r1 r1
  write 4 r14 r1
  read 4 r14 r1
  beq r1 r0 Label_L604
  read -4 r14 r1
  read -5 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  xor r1 -1 r1
  bne r1 r0 Label_L606
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_free_block
  sub r13 -4 r13
  write -10 r14 r1
  read -10 r14 r1
  bges r1 r0 Label_L609
  read -2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -2 r14 r8
  add r8 2 r8
  read 0 r8 r8
  add r8 7 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L611
  read -2 r14 r1
  add r1 2 r1
  read 0 r1 r1
  add r1 7 r1
  read -2 r14 r8
  add r8 1 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L611:
  read -9 r14 r1
  jump Label_L581
Label_L609:
  read -4 r14 r1
  read -5 r14 r8
  add r1 r8 r1
  read -10 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read -4 r14 r1
  read -10 r14 r8
  add r1 r8 r1
  load32 -1 r8
  write 0 r1 r8
  or r8 r0 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  push r1
; Stack depth: 1
  load32 0 r1
  push r1
; Stack depth: 2
  read -10 r14 r1
  push r1
; Stack depth: 3
  pop r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  push r1
; Stack depth: 4
  pop r4
  pop r5
  pop r6
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read -10 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  read -10 r14 r1
  write -5 r14 r1
  jump Label_L607
Label_L606:
  read -4 r14 r1
  read -5 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -5 r14 r1
Label_L607:
Label_L604:
  jump Label_L600
Label_L601:
  read -2 r14 r1
  add r1 1 r1
  read 0 r1 r1
  read -2 r14 r8
  add r8 2 r8
  read 0 r8 r8
  add r8 7 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L615
  read -2 r14 r1
  add r1 2 r1
  read 0 r1 r1
  add r1 7 r1
  read -2 r14 r8
  add r8 1 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L615:
  read -9 r14 r1
Label_L581:
  read 1 r14 r15
  read 0 r14 r14
  add r13 13 r13
  jumpr 0 r15

.code
Label_brfs_seek:
  write 0 r13 r4
  write 1 r13 r5
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L619
  load32 -19 r1
  jump Label_L618
Label_L619:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L623
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L623:
  beq r1 r0 Label_L621
  load32 -1 r1
  jump Label_L618
Label_L621:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -1 r14 r1
  read -1 r14 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L624
  load32 -8 r1
  jump Label_L618
Label_L624:
  read 3 r14 r1
  read -1 r14 r8
  add r8 2 r8
  read 0 r8 r8
  add r8 7 r8
  read 0 r8 r8
  sltu r8 r1 r1
  beq r1 r0 Label_L627
  read -1 r14 r1
  add r1 2 r1
  read 0 r1 r1
  add r1 7 r1
  read 0 r1 r1
  write 3 r14 r1
Label_L627:
  read -1 r14 r1
  add r1 1 r1
  read 3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r1
Label_L618:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_brfs_tell:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L631
  load32 -19 r1
  jump Label_L630
Label_L631:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L635
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L635:
  beq r1 r0 Label_L633
  load32 -1 r1
  jump Label_L630
Label_L633:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -1 r14 r1
  read -1 r14 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L636
  load32 -8 r1
  jump Label_L630
Label_L636:
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
Label_L630:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_brfs_file_size:
  write 0 r13 r4
  sub r13          3 r13
  write          1 r13 r14
  add r13          1 r14
  ;write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L641
  load32 -19 r1
  jump Label_L640
Label_L641:
  read 2 r14 r1
  slt r1 r0 r1
  bne r1 r0 Label_L645
  read 2 r14 r1
  slt r1 16 r1
  xor r1 1 r1
Label_L645:
  beq r1 r0 Label_L643
  load32 -1 r1
  jump Label_L640
Label_L643:
  addr2reg Label_brfs r1
  add r1 7 r1
  read 2 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  write -1 r14 r1
  read -1 r14 r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L646
  load32 -8 r1
  jump Label_L640
Label_L646:
  read -1 r14 r1
  add r1 2 r1
  read 0 r1 r1
  add r1 7 r1
  read 0 r1 r1
Label_L640:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_brfs_read_dir:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          9 r13
  write          7 r13 r14
  add r13          7 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L651
  load32 -19 r1
  jump Label_L650
Label_L651:
  read 3 r14 r1
  bne r1 r0 Label_L653
  load32 -1 r1
  jump Label_L650
Label_L653:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -2 r14 r1
  read -2 r14 r1
  bges r1 r0 Label_L656
  read -2 r14 r1
  jump Label_L650
Label_L656:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  read -2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -3 r14 r1
  read -1 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -4 r14 r1
  write -5 r14 r0
  write -6 r14 r0
Label_L659:
  read -6 r14 r1
  read -4 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L663
  read -5 r14 r1
  read 4 r14 r8
  sltu r1 r8 r1
Label_L663:
  beq r1 r0 Label_L662
  read -3 r14 r1
  read -6 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -7 r14 r1
  read -7 r14 r1
  add r1 0 r1
  read 0 r1 r1
  beq r1 r0 Label_L665
  load32 8 r6
  read -7 r14 r5
  read 3 r14 r4
  push r4
; Stack depth: 1
  read -5 r14 r4
  shiftl r4 3 r4
  pop r11
; Stack depth: 0
  add r11 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  read -5 r14 r1
  add r1 1 r1
  write -5 r14 r1
  add r1 -1 r1
Label_L665:
Label_L660:
  read -6 r14 r1
  add r1 1 r1
  write -6 r14 r1
  add r1 -1 r1
  jump Label_L659
Label_L662:
  read -5 r14 r1
Label_L650:
  read 1 r14 r15
  read 0 r14 r14
  add r13 9 r13
  jumpr 0 r15

.code
Label_brfs_delete:
  write 0 r13 r4
  sub r13        159 r13
  write        157 r13 r14
  add r13        157 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L670
  load32 -19 r1
  jump Label_L669
Label_L670:
  load32 128 r7
  add r14 -145 r6
  add r14 -128 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L672
  read -146 r14 r1
  jump Label_L669
Label_L672:
  add r14 -128 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -147 r14 r1
  read -147 r14 r1
  bges r1 r0 Label_L674
  read -147 r14 r1
  jump Label_L669
Label_L674:
  add r14 -148 r6
  add r14 -145 r5
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L676
  read -146 r14 r1
  jump Label_L669
Label_L676:
  read -148 r14 r1
  add r1 5 r1
  read 0 r1 r1
  and r1 1 r1
  beq r1 r0 Label_L678
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -153 r14 r1
  read -148 r14 r4
  add r4 6 r4
  read 0 r4 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_data_block
  sub r13 -4 r13
  write -154 r14 r1
  read -153 r14 r1
  add r1 1 r1
  read 0 r1 r1
  shiftr r1 3 r1
  write -155 r14 r1
  write -157 r14 r0
  write -152 r14 r0
Label_L681:
  read -152 r14 r1
  read -155 r14 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L684
  read -154 r14 r1
  read -152 r14 r8
  shiftl r8 3 r8
  add r1 r8 r1
  write -156 r14 r1
  read -156 r14 r1
  add r1 0 r1
  read 0 r1 r1
  beq r1 r0 Label_L686
  read -157 r14 r1
  add r1 1 r1
  write -157 r14 r1
  add r1 -1 r1
Label_L686:
Label_L682:
  read -152 r14 r1
  add r1 1 r1
  write -152 r14 r1
  add r1 -1 r1
  jump Label_L681
Label_L684:
  read -157 r14 r1
  sltu r1 3 r1
  bne r1 r0 Label_L688
  load32 -6 r1
  jump Label_L669
Label_L688:
Label_L678:
  write -152 r14 r0
Label_L690:
  read -152 r14 r1
  sltu r1 16 r1
  beq r1 r0 Label_L693
  addr2reg Label_brfs r1
  add r1 7 r1
  read -152 r14 r8
  load32 3 r9
  mults r8 r9 r8
  add r1 r8 r1
  add r1 2 r1
  read 0 r1 r1
  read -148 r14 r8
  bne r1 r8 Label_L694
  load32 -7 r1
  jump Label_L669
Label_L694:
Label_L691:
  read -152 r14 r1
  add r1 1 r1
  write -152 r14 r1
  add r1 -1 r1
  jump Label_L690
Label_L693:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -149 r14 r1
  read -148 r14 r1
  add r1 6 r1
  read 0 r1 r1
  write -150 r14 r1
Label_L696:
  read -150 r14 r1
  xor r1 -1 r1
  beq r1 r0 Label_L697
  read -149 r14 r1
  read -150 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  write -151 r14 r1
  read -149 r14 r1
  read -150 r14 r8
  add r1 r8 r1
  write 0 r1 r0
  read -150 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  read -151 r14 r1
  write -150 r14 r1
  jump Label_L696
Label_L697:
  load32 8 r6
  load32 0 r5
  read -148 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_mark_block_dirty
  sub r13 -4 r13
  load32 0 r1
Label_L669:
  read 1 r14 r15
  read 0 r14 r14
  add r13 159 r13
  jumpr 0 r15

.code
Label_brfs_stat:
  write 0 r13 r4
  write 1 r13 r5
  sub r13        152 r13
  write        150 r13 r14
  add r13        150 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L701
  load32 -19 r1
  jump Label_L700
Label_L701:
  read 3 r14 r1
  bne r1 r0 Label_L703
  load32 -1 r1
  jump Label_L700
Label_L703:
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strlen
  sub r13 -4 r13
  write -149 r14 r1
  read -149 r14 r1
  sltu r1 1 r1
  bne r1 r0 Label_L708
  read -149 r14 r1
  xor r1 1 r1
  sltu r1 1 r1
  beq r1 r0 Label_L709
  read 2 r14 r1
  add r1 0 r1
  read 0 r1 r1
  shiftl r1 24 r1
  shiftrs r1 24 r1
  xor r1 47 r1
  sltu r1 1 r1
Label_L709:
  sltu r0 r1 r1
Label_L708:
  beq r1 r0 Label_L706
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -150 r14 r1
  load32 8 r6
  load32 0 r5
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memset
  sub r13 -4 r13
  read 3 r14 r1
  add r1 5 r1
  load32 1 r8
  write 0 r1 r8
  or r8 r0 r1
  read 3 r14 r1
  add r1 6 r1
  write 0 r1 r0
  read 3 r14 r1
  add r1 7 r1
  read -150 r14 r8
  add r8 1 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1

.rdata
Label_L712:
  .dsw "/"
  .dw 0

.code
  addr2reg Label_L712 r5
  read 3 r14 r4
  add r4 0 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_compress_string
  sub r13 -4 r13
  load32 0 r1
  jump Label_L700
Label_L706:
  load32 128 r7
  add r14 -145 r6
  add r14 -128 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L713
  read -146 r14 r1
  jump Label_L700
Label_L713:
  add r14 -128 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_dir_fat_idx
  sub r13 -4 r13
  write -147 r14 r1
  read -147 r14 r1
  bges r1 r0 Label_L715
  read -147 r14 r1
  jump Label_L700
Label_L715:
  add r14 -148 r6
  add r14 -145 r5
  read -147 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_find_in_directory
  sub r13 -4 r13
  write -146 r14 r1
  read -146 r14 r1
  beq r1 r0 Label_L717
  read -146 r14 r1
  jump Label_L700
Label_L717:
  load32 8 r6
  read -148 r14 r5
  read 3 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_memcpy
  sub r13 -4 r13
  load32 0 r1
Label_L700:
  read 1 r14 r15
  read 0 r14 r14
  add r13 152 r13
  jumpr 0 r15

.code
Label_brfs_exists:
  write 0 r13 r4
  sub r13         10 r13
  write          8 r13 r14
  add r13          8 r14
   write 1 r14 r15
  add r14 -8 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_stat
  sub r13 -4 r13
  sltu r1 1 r1
  bne r1 r0 Label_L721
  load32 0 r1
  jump Label_L722
Label_L721:
  load32 1 r1
Label_L722:
Label_L720:
  read 1 r14 r15
  read 0 r14 r14
  add r13 10 r13
  jumpr 0 r15

.code
Label_brfs_is_dir:
  write 0 r13 r4
  sub r13         10 r13
  write          8 r13 r14
  add r13          8 r14
   write 1 r14 r15
  add r14 -8 r5
  read 2 r14 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_stat
  sub r13 -4 r13
  beq r1 r0 Label_L724
  load32 0 r1
  jump Label_L723
Label_L724:
  read -3 r14 r1
  and r1 1 r1
  bne r1 r0 Label_L726
  load32 0 r1
  jump Label_L727
Label_L726:
  load32 1 r1
Label_L727:
Label_L723:
  read 1 r14 r15
  read 0 r14 r14
  add r13 10 r13
  jumpr 0 r15

.code
Label_brfs_statfs:
  write 0 r13 r4
  write 1 r13 r5
  write 2 r13 r6
  sub r13          6 r13
  write          4 r13 r14
  add r13          4 r14
   write 1 r14 r15
  addr2reg Label_brfs r1
  add r1 2 r1
  read 0 r1 r1
  bne r1 r0 Label_L729
  load32 -19 r1
  jump Label_L728
Label_L729:
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_superblock
  sub r13 -4 r13
  write -1 r14 r1
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_get_fat
  sub r13 -4 r13
  write -2 r14 r1
  write -3 r14 r0
  write -4 r14 r0
Label_L732:
  read -4 r14 r1
  read -1 r14 r8
  add r8 0 r8
  read 0 r8 r8
  sltu r1 r8 r1
  beq r1 r0 Label_L735
  read -2 r14 r1
  read -4 r14 r8
  add r1 r8 r1
  read 0 r1 r1
  bne r1 r0 Label_L736
  read -3 r14 r1
  add r1 1 r1
  write -3 r14 r1
  add r1 -1 r1
Label_L736:
Label_L733:
  read -4 r14 r1
  add r1 1 r1
  write -4 r14 r1
  add r1 -1 r1
  jump Label_L732
Label_L735:
  read 2 r14 r1
  beq r1 r0 Label_L738
  read 2 r14 r1
  read -1 r14 r8
  add r8 0 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L738:
  read 3 r14 r1
  beq r1 r0 Label_L741
  read 3 r14 r1
  read -3 r14 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L741:
  read 4 r14 r1
  beq r1 r0 Label_L744
  read 4 r14 r1
  read -1 r14 r8
  add r8 1 r8
  read 0 r8 r8
  write 0 r1 r8
  or r8 r0 r1
Label_L744:
  load32 0 r1
Label_L728:
  read 1 r14 r15
  read 0 r14 r14
  add r13 6 r13
  jumpr 0 r15

.code
Label_brfs_strerror:
  write 0 r13 r4
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
  read 2 r14 r1
  jump Label_L749
Label_L750:

.rdata
Label_L751:
  .dsw "Success"
  .dw 0

.code
  addr2reg Label_L751 r1
  jump Label_L747
Label_L752:

.rdata
Label_L753:
  .dsw "Invalid parameter"
  .dw 0

.code
  addr2reg Label_L753 r1
  jump Label_L747
Label_L754:

.rdata
Label_L755:
  .dsw "Not found"
  .dw 0

.code
  addr2reg Label_L755 r1
  jump Label_L747
Label_L756:

.rdata
Label_L757:
  .dsw "Already exists"
  .dw 0

.code
  addr2reg Label_L757 r1
  jump Label_L747
Label_L758:

.rdata
Label_L759:
  .dsw "No space left"
  .dw 0

.code
  addr2reg Label_L759 r1
  jump Label_L747
Label_L760:

.rdata
Label_L761:
  .dsw "No free directory entry"
  .dw 0

.code
  addr2reg Label_L761 r1
  jump Label_L747
Label_L762:

.rdata
Label_L763:
  .dsw "Directory not empty"
  .dw 0

.code
  addr2reg Label_L763 r1
  jump Label_L747
Label_L764:

.rdata
Label_L765:
  .dsw "File is open"
  .dw 0

.code
  addr2reg Label_L765 r1
  jump Label_L747
Label_L766:

.rdata
Label_L767:
  .dsw "File is not open"
  .dw 0

.code
  addr2reg Label_L767 r1
  jump Label_L747
Label_L768:

.rdata
Label_L769:
  .dsw "Too many open files"
  .dw 0

.code
  addr2reg Label_L769 r1
  jump Label_L747
Label_L770:

.rdata
Label_L771:
  .dsw "Is a directory"
  .dw 0

.code
  addr2reg Label_L771 r1
  jump Label_L747
Label_L772:

.rdata
Label_L773:
  .dsw "Not a directory"
  .dw 0

.code
  addr2reg Label_L773 r1
  jump Label_L747
Label_L774:

.rdata
Label_L775:
  .dsw "Path too long"
  .dw 0

.code
  addr2reg Label_L775 r1
  jump Label_L747
Label_L776:

.rdata
Label_L777:
  .dsw "Filename too long"
  .dw 0

.code
  addr2reg Label_L777 r1
  jump Label_L747
Label_L778:

.rdata
Label_L779:
  .dsw "Invalid superblock"
  .dw 0

.code
  addr2reg Label_L779 r1
  jump Label_L747
Label_L780:

.rdata
Label_L781:
  .dsw "Flash error"
  .dw 0

.code
  addr2reg Label_L781 r1
  jump Label_L747
Label_L782:

.rdata
Label_L783:
  .dsw "Seek error"
  .dw 0

.code
  addr2reg Label_L783 r1
  jump Label_L747
Label_L784:

.rdata
Label_L785:
  .dsw "Read error"
  .dw 0

.code
  addr2reg Label_L785 r1
  jump Label_L747
Label_L786:

.rdata
Label_L787:
  .dsw "Write error"
  .dw 0

.code
  addr2reg Label_L787 r1
  jump Label_L747
Label_L788:

.rdata
Label_L789:
  .dsw "Not initialized"
  .dw 0

.code
  addr2reg Label_L789 r1
  jump Label_L747
Label_L790:

.rdata
Label_L791:
  .dsw "Unknown error"
  .dw 0

.code
  addr2reg Label_L791 r1
  jump Label_L747
  jump Label_L748
Label_L749:
  load32 0 r12
  beq r1 r12 Label_L750
  load32 -1 r12
  beq r1 r12 Label_L752
  load32 -2 r12
  beq r1 r12 Label_L754
  load32 -3 r12
  beq r1 r12 Label_L756
  load32 -4 r12
  beq r1 r12 Label_L758
  load32 -5 r12
  beq r1 r12 Label_L760
  load32 -6 r12
  beq r1 r12 Label_L762
  load32 -7 r12
  beq r1 r12 Label_L764
  load32 -8 r12
  beq r1 r12 Label_L766
  load32 -9 r12
  beq r1 r12 Label_L768
  load32 -10 r12
  beq r1 r12 Label_L770
  load32 -11 r12
  beq r1 r12 Label_L772
  load32 -12 r12
  beq r1 r12 Label_L774
  load32 -13 r12
  beq r1 r12 Label_L776
  load32 -14 r12
  beq r1 r12 Label_L778
  load32 -15 r12
  beq r1 r12 Label_L780
  load32 -16 r12
  beq r1 r12 Label_L782
  load32 -17 r12
  beq r1 r12 Label_L784
  load32 -18 r12
  beq r1 r12 Label_L786
  load32 -19 r12
  beq r1 r12 Label_L788
  jump Label_L790
Label_L748:
Label_L747:
  read 0 r14 r14
  add r13 2 r13
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
Label_L792:
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
Label_L793:
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
Label_L795:
  read 0 r14 r14
  add r13 3 r13
  jumpr 0 r15

.code
Label_main:
  sub r13         52 r13
  write         50 r13 r14
  add r13         50 r14
   write 1 r14 r15

.rdata
Label_L798:
  .dsw "/test.txt"
  .dw 0

.code
  load32 32 r7
  add r14 -49 r6
  add r14 -32 r5
  addr2reg Label_L798 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -50 r14 r1
  read -50 r14 r1
  beq r1 r0 Label_L799
  load32 1 r1
  jump Label_L797
Label_L799:

.rdata
Label_L803:
  .dsw "/"
  .dw 0

.code
  addr2reg Label_L803 r5
  add r14 -32 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L801
  load32 2 r1
  jump Label_L797
Label_L801:

.rdata
Label_L806:
  .dsw "test.txt"
  .dw 0

.code
  addr2reg Label_L806 r5
  add r14 -49 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L804
  load32 3 r1
  jump Label_L797
Label_L804:

.rdata
Label_L807:
  .dsw "myfile.c"
  .dw 0

.code
  load32 32 r7
  add r14 -49 r6
  add r14 -32 r5
  addr2reg Label_L807 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -50 r14 r1
  read -50 r14 r1
  beq r1 r0 Label_L808
  load32 4 r1
  jump Label_L797
Label_L808:

.rdata
Label_L812:
  .dsw "/"
  .dw 0

.code
  addr2reg Label_L812 r5
  add r14 -32 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L810
  load32 5 r1
  jump Label_L797
Label_L810:

.rdata
Label_L815:
  .dsw "myfile.c"
  .dw 0

.code
  addr2reg Label_L815 r5
  add r14 -49 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L813
  load32 6 r1
  jump Label_L797
Label_L813:

.rdata
Label_L816:
  .dsw "/sub/file.txt"
  .dw 0

.code
  load32 32 r7
  add r14 -49 r6
  add r14 -32 r5
  addr2reg Label_L816 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -50 r14 r1
  read -50 r14 r1
  beq r1 r0 Label_L817
  load32 7 r1
  jump Label_L797
Label_L817:

.rdata
Label_L821:
  .dsw "/sub"
  .dw 0

.code
  addr2reg Label_L821 r5
  add r14 -32 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L819
  load32 8 r1
  jump Label_L797
Label_L819:

.rdata
Label_L824:
  .dsw "file.txt"
  .dw 0

.code
  addr2reg Label_L824 r5
  add r14 -49 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L822
  load32 9 r1
  jump Label_L797
Label_L822:

.rdata
Label_L825:
  .dsw "/a/b/test.dat"
  .dw 0

.code
  load32 32 r7
  add r14 -49 r6
  add r14 -32 r5
  addr2reg Label_L825 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_brfs_parse_path
  sub r13 -4 r13
  write -50 r14 r1
  read -50 r14 r1
  beq r1 r0 Label_L826
  load32 10 r1
  jump Label_L797
Label_L826:

.rdata
Label_L830:
  .dsw "/a/b"
  .dw 0

.code
  addr2reg Label_L830 r5
  add r14 -32 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L828
  load32 11 r1
  jump Label_L797
Label_L828:

.rdata
Label_L833:
  .dsw "test.dat"
  .dw 0

.code
  addr2reg Label_L833 r5
  add r14 -49 r4
  sub r13 4 r13
  savpc r15
  add r15 3 r15
  jump Label_strcmp
  sub r13 -4 r13
  beq r1 r0 Label_L831
  load32 12 r1
  jump Label_L797
Label_L831:
  load32 42 r1
  jump Label_L797
  load32 0 r1
Label_L797:
  read 1 r14 r15
  read 0 r14 r14
  add r13 52 r13
  jumpr 0 r15

.code
Label_interrupt:
  sub r13          2 r13
  write          0 r13 r14
  add r13          0 r14
  ;write 1 r14 r15
Label_L834:
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
