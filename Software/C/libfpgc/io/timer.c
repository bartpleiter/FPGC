#include "fpgc.h"
#include "timer.h"
#include <stddef.h>

/* Timer register addresses */
static const int timer_val_addrs[TIMER_COUNT] = {
    FPGC_TIMER0_VAL, FPGC_TIMER1_VAL, FPGC_TIMER2_VAL
};

static const int timer_ctrl_addrs[TIMER_COUNT] = {
    FPGC_TIMER0_CTRL, FPGC_TIMER1_CTRL, FPGC_TIMER2_CTRL
};

/* Timer state */
typedef struct {
    timer_callback_t callback;
    unsigned int period_ms;
    int active;
} timer_state_t;

static timer_state_t timer_state[TIMER_COUNT];
static int delay_complete = 0;

static int
timer_valid(int id)
{
    return (id >= 0 && id < TIMER_COUNT);
}

void
timer_init(void)
{
    int i;
    for (i = 0; i < TIMER_COUNT; i++) {
        timer_state[i].callback = (timer_callback_t)0;
        timer_state[i].period_ms = 0;
        timer_state[i].active = 0;
    }
    delay_complete = 0;
}

void
timer_set(int timer_id, unsigned int ms)
{
    if (!timer_valid(timer_id))
        return;
    __builtin_store(timer_val_addrs[timer_id], (int)ms);
}

void
timer_start(int timer_id)
{
    if (!timer_valid(timer_id))
        return;
    __builtin_store(timer_ctrl_addrs[timer_id], 1);
}

void
timer_start_ms(int timer_id, unsigned int ms)
{
    if (!timer_valid(timer_id))
        return;
    timer_state[timer_id].period_ms = 0;
    timer_state[timer_id].active = 1;
    timer_set(timer_id, ms);
    timer_start(timer_id);
}

unsigned int
timer_get_period(int timer_id)
{
    if (!timer_valid(timer_id))
        return 0;
    return timer_state[timer_id].period_ms;
}

void
timer_set_callback(int timer_id, timer_callback_t callback)
{
    if (!timer_valid(timer_id))
        return;
    timer_state[timer_id].callback = callback;
}

void
timer_start_periodic(int timer_id, unsigned int period_ms)
{
    if (!timer_valid(timer_id))
        return;
    if (period_ms == 0)
        return;
    timer_state[timer_id].period_ms = period_ms;
    timer_state[timer_id].active = 1;
    timer_set(timer_id, period_ms);
    timer_start(timer_id);
}

void
timer_cancel(int timer_id)
{
    if (!timer_valid(timer_id))
        return;
    timer_state[timer_id].callback = (timer_callback_t)0;
    timer_state[timer_id].period_ms = 0;
    timer_state[timer_id].active = 0;
}

int
timer_is_active(int timer_id)
{
    if (!timer_valid(timer_id))
        return 0;
    return timer_state[timer_id].active;
}

void
timer_isr_handler(int timer_id)
{
    timer_callback_t cb;
    unsigned int period;

    if (!timer_valid(timer_id))
        return;

    if (timer_id == TIMER_DELAY)
        delay_complete = 1;

    if (!timer_state[timer_id].active)
        return;

    cb = timer_state[timer_id].callback;
    period = timer_state[timer_id].period_ms;

    if (cb != (timer_callback_t)0)
        cb(timer_id);

    if (period > 0) {
        timer_set(timer_id, period);
        timer_start(timer_id);
    } else {
        timer_state[timer_id].active = 0;
    }
}

void
delay(unsigned int ms)
{
    timer_callback_t saved_cb;
    unsigned int saved_period;
    int saved_active;

    if (ms == 0)
        return;

    saved_cb = timer_state[TIMER_DELAY].callback;
    saved_period = timer_state[TIMER_DELAY].period_ms;
    saved_active = timer_state[TIMER_DELAY].active;

    timer_state[TIMER_DELAY].callback = (timer_callback_t)0;
    timer_state[TIMER_DELAY].period_ms = 0;
    timer_state[TIMER_DELAY].active = 1;

    delay_complete = 0;
    timer_set(TIMER_DELAY, ms);
    timer_start(TIMER_DELAY);

    while (!delay_complete) {
        /* busy wait for timer_isr_handler() */
    }

    timer_state[TIMER_DELAY].callback = saved_cb;
    timer_state[TIMER_DELAY].period_ms = saved_period;
    timer_state[TIMER_DELAY].active = saved_active;
}
