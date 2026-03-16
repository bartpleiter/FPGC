#ifndef FPGC_SYS_H
#define FPGC_SYS_H

/* Read the current interrupt identifier (assembly: readintid) */
int get_int_id(void);

/* Read the boot mode register */
int get_boot_mode(void);

/* Set user LED state (0=off, nonzero=on) */
void set_user_led(int on);

/* Read the hardware microsecond counter */
unsigned int get_micros(void);

#endif /* FPGC_SYS_H */
