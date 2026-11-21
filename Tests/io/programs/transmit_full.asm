; Writes 40 bytes to the UART as fast as it can. 33 fit, 32 in the buffer and one in the
; transmitter, and from the 34th on, each write waits until a byte has been sent: the
; bus answers it later.
; devices
; uart output "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVW"
    load 0x30 r1
    load 0x58 r2
Next:
    write 0xFFFF8000 r0 r1      ; TRANSMIT
    add r1 1 r1
    bne r1 r2 Next
    read 0xFFFF8008 r0 r3       ; STATUS: the transmit buffer is full
    halt
