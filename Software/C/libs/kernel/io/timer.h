#ifndef TIMER_H
#define TIMER_H

/*
 * Timer library for hardware timer management
 * Provides functions to configure and control three hardware timers (0, 1, 2).
 * 
 * Hardware capabilities:
 * - Each timer has a value register (time in ms) and a control/trigger register
 * - Writing to value register sets the countdown time
 * - Writing 1 to control register starts the timer
 * - Timer fires interrupt when countdown reaches 0
 * - Hardware limitation: Cannot stop a running timer or check if running
 * 
 * This library provides:
 * - Basic timer control (set, start)
 * - Callback registration for timer completion
 * - Periodic timer support (auto-restart)
 * - Blocking delay function
 * 
 * Usage in interrupt handler:
 *   void interrupt() {
 *       int id = get_int_id();
 *       switch (id) {
 *           case INTID_TIMER0: timer_isr_handler(TIMER_0); break;
 *           case INTID_TIMER1: timer_isr_handler(TIMER_1); break;
 *           case INTID_TIMER2: timer_isr_handler(TIMER_2); break;
 *       }
 *   }
 */

/* Timer identifiers */
#define TIMER_0     0
#define TIMER_1     1
#define TIMER_2     2

/* Number of available timers */
#define TIMER_COUNT 3

/* Timer used internally by delay() - users should avoid using this timer if using delay() */
#define TIMER_DELAY TIMER_2

/* Callback function pointer type */
typedef void (*timer_callback_t)(int timer_id);

/**
 * Initialize the timer subsystem.
 * Clears all callbacks and periodic settings.
 * Call this once at program startup.
 */
void timer_init(void);

/**
 * Set the timer value in milliseconds (does not start the timer).
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @param ms Time to wait in milliseconds
 */
void timer_set(int timer_id, unsigned int ms);

/**
 * Start the timer (uses previously set value).
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 */
void timer_start(int timer_id);

/**
 * Configure and start a timer in one call (one-shot mode).
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @param ms Time to wait in milliseconds
 */
void timer_start_ms(int timer_id, unsigned int ms);

/**
 * Get the configured period for a timer.
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @return Configured period in ms (0 if one-shot or not configured)
 */
unsigned int timer_get_period(int timer_id);

/**
 * Register a callback function for timer completion.
 * The callback will be called from the interrupt handler context.
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @param callback Function to call when timer fires (NULL to clear)
 */
void timer_set_callback(int timer_id, timer_callback_t callback);

/**
 * Start a periodic timer that automatically restarts after each completion.
 * The callback (if set) will be called each time the timer fires.
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @param period_ms Period in milliseconds
 */
void timer_start_periodic(int timer_id, unsigned int period_ms);

/**
 * Cancel a timer's periodic mode and callback.
 * Note: Due to hardware limitations, if the timer is currently running,
 * it will still complete and fire one more interrupt, but the callback
 * won't be called and it won't restart.
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 */
void timer_cancel(int timer_id);

/**
 * Check if a timer is configured as active (periodic or waiting for callback).
 * Note: This is a software state, not hardware state.
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 * @return 1 if timer is active, 0 otherwise
 */
int timer_is_active(int timer_id);

/**
 * Timer ISR handler. MUST be called from the interrupt handler
 * for each timer interrupt.
 * 
 * This function:
 * - Calls the registered callback (if any)
 * - Restarts the timer if in periodic mode
 * - Handles internal delay() completion
 * 
 * @param timer_id Timer ID (TIMER_0, TIMER_1, or TIMER_2)
 */
void timer_isr_handler(int timer_id);

/**
 * Blocking delay using TIMER_DELAY (Timer 2 by default).
 * 
 * Note: This function uses TIMER_DELAY internally. Do not use that
 * timer for other purposes while delay() is active. The interrupt
 * handler must call timer_isr_handler(TIMER_DELAY) for this to work.
 * 
 * @param ms Delay time in milliseconds
 */
void delay(unsigned int ms);

#endif // TIMER_H
