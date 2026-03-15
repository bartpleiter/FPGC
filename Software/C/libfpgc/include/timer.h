/*
 * timer.h — Hardware timer driver for B32P3/FPGC.
 *
 * Three hardware countdown timers with callback support and blocking delay.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FPGC_TIMER_H
#define FPGC_TIMER_H

#define TIMER_0     0
#define TIMER_1     1
#define TIMER_2     2
#define TIMER_COUNT 3

/* Timer used by delay() — avoid using this timer for other purposes if using delay() */
#define TIMER_DELAY TIMER_2

typedef void (*timer_callback_t)(int timer_id);

void         timer_init(void);
void         timer_set(int timer_id, unsigned int ms);
void         timer_start(int timer_id);
void         timer_start_ms(int timer_id, unsigned int ms);
unsigned int timer_get_period(int timer_id);
void         timer_set_callback(int timer_id, timer_callback_t callback);
void         timer_start_periodic(int timer_id, unsigned int period_ms);
void         timer_cancel(int timer_id);
int          timer_is_active(int timer_id);
void         timer_isr_handler(int timer_id);
void         delay(unsigned int ms);

#endif /* FPGC_TIMER_H */
