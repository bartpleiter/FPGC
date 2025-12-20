#include "libs/kernel/io/timer.h"

/* Hardware register addresses */
#define TIMER0_VAL   0x7000002
#define TIMER0_CTRL  0x7000003
#define TIMER1_VAL   0x7000004
#define TIMER1_CTRL  0x7000005
#define TIMER2_VAL   0x7000006
#define TIMER2_CTRL  0x7000007

/* Timer state structure */
typedef struct {
    timer_callback_t callback;  /* User callback function */
    unsigned int period_ms;     /* Period for periodic mode (0 = one-shot) */
    int active;                 /* Software active flag */
} timer_state_t;

/* State for all timers */
static timer_state_t timer_state[TIMER_COUNT];

/* Internal flag for delay() function */
static volatile int delay_complete = 0;

/* Helper: Get pointer to timer value register */
static int* timer_val_addr(int timer_id)
{
    if (timer_id == TIMER_0) return (int*)TIMER0_VAL;
    if (timer_id == TIMER_1) return (int*)TIMER1_VAL;
    if (timer_id == TIMER_2) return (int*)TIMER2_VAL;
    return (int*)TIMER0_VAL; /* Default to timer 0 */
}

/* Helper: Get pointer to timer control register */
static int* timer_ctrl_addr(int timer_id)
{
    if (timer_id == TIMER_0) return (int*)TIMER0_CTRL;
    if (timer_id == TIMER_1) return (int*)TIMER1_CTRL;
    if (timer_id == TIMER_2) return (int*)TIMER2_CTRL;
    return (int*)TIMER0_CTRL; /* Default to timer 0 */
}

/* Helper: Validate timer ID */
static int timer_valid(int timer_id)
{
    return (timer_id >= 0 && timer_id < TIMER_COUNT);
}

void timer_init(void)
{
    int i;
    for (i = 0; i < TIMER_COUNT; i++) {
        timer_state[i].callback = (timer_callback_t)0;
        timer_state[i].period_ms = 0;
        timer_state[i].active = 0;
    }
    delay_complete = 0;
}

void timer_set(int timer_id, unsigned int ms)
{
    int* val_addr;
    if (!timer_valid(timer_id)) return;
    
    val_addr = timer_val_addr(timer_id);
    *val_addr = ms;
}

void timer_start(int timer_id)
{
    int* ctrl_addr;
    if (!timer_valid(timer_id)) return;
    
    ctrl_addr = timer_ctrl_addr(timer_id);
    *ctrl_addr = 1;
}

void timer_start_ms(int timer_id, unsigned int ms)
{
    if (!timer_valid(timer_id)) return;
    
    timer_state[timer_id].period_ms = 0;  /* One-shot mode */
    timer_state[timer_id].active = 1;
    timer_set(timer_id, ms);
    timer_start(timer_id);
}

unsigned int timer_get_period(int timer_id)
{
    if (!timer_valid(timer_id)) return 0;
    return timer_state[timer_id].period_ms;
}

void timer_set_callback(int timer_id, timer_callback_t callback)
{
    if (!timer_valid(timer_id)) return;
    timer_state[timer_id].callback = callback;
}

void timer_start_periodic(int timer_id, unsigned int period_ms)
{
    if (!timer_valid(timer_id)) return;
    if (period_ms == 0) return;  /* Invalid period */
    
    timer_state[timer_id].period_ms = period_ms;
    timer_state[timer_id].active = 1;
    timer_set(timer_id, period_ms);
    timer_start(timer_id);
}

void timer_cancel(int timer_id)
{
    if (!timer_valid(timer_id)) return;
    
    timer_state[timer_id].callback = (timer_callback_t)0;
    timer_state[timer_id].period_ms = 0;
    timer_state[timer_id].active = 0;
}

int timer_is_active(int timer_id)
{
    if (!timer_valid(timer_id)) return 0;
    return timer_state[timer_id].active;
}

void timer_isr_handler(int timer_id)
{
    timer_callback_t cb;
    unsigned int period;
    
    if (!timer_valid(timer_id)) return;
    
    /* Handle delay() completion for TIMER_DELAY */
    if (timer_id == TIMER_DELAY) {
        delay_complete = 1;
    }
    
    /* Check if timer is active (not cancelled) */
    if (!timer_state[timer_id].active) {
        return;
    }
    
    /* Get callback and period before potentially being modified */
    cb = timer_state[timer_id].callback;
    period = timer_state[timer_id].period_ms;
    
    /* Call user callback if registered */
    if (cb != (timer_callback_t)0) {
        cb(timer_id);
    }
    
    /* Restart if periodic */
    if (period > 0) {
        timer_set(timer_id, period);
        timer_start(timer_id);
    } else {
        /* One-shot: mark as inactive */
        timer_state[timer_id].active = 0;
    }
}

void delay(unsigned int ms)
{
    if (ms == 0) return;
    
    /* Save and clear any existing callback for TIMER_DELAY */
    timer_callback_t saved_cb = timer_state[TIMER_DELAY].callback;
    unsigned int saved_period = timer_state[TIMER_DELAY].period_ms;
    int saved_active = timer_state[TIMER_DELAY].active;
    
    /* Configure for delay */
    timer_state[TIMER_DELAY].callback = (timer_callback_t)0;
    timer_state[TIMER_DELAY].period_ms = 0;
    timer_state[TIMER_DELAY].active = 1;
    
    delay_complete = 0;
    timer_set(TIMER_DELAY, ms);
    timer_start(TIMER_DELAY);
    
    /* Busy wait for timer interrupt */
    while (!delay_complete) {
        /* Wait for timer_isr_handler() to be called */
    }
    
    /* Restore previous state */
    timer_state[TIMER_DELAY].callback = saved_cb;
    timer_state[TIMER_DELAY].period_ms = saved_period;
    timer_state[TIMER_DELAY].active = saved_active;
}


