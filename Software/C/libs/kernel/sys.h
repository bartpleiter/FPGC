#ifndef SYS_H
#define SYS_H

// System-level definitions and utilities

#define INTID_UART 1
#define INTID_TIMER0 2
#define INTID_TIMER1 3
#define INTID_TIMER2 4
#define INTID_FRAME_DRAWN 5

int get_int_id();

int get_boot_mode();

unsigned int get_micros();

void set_user_led(int on);

#endif // SYS_H
