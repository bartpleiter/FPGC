#define COMMON_STDLIB
#include "libs/common/common.h"

#define KERNEL_TIMER
#define KERNEL_UART
#include "libs/kernel/kernel.h"

/* Counter for periodic timer test */
volatile int periodic_count = 0;

/* Flag for one-shot callback test */
volatile int oneshot_fired = 0;

/* Callback for periodic timer */
void periodic_callback(int timer_id)
{
    periodic_count++;
    uart_puts("Tick ");
    uart_putint(periodic_count);
    uart_puts("\n");
}

/* Callback for one-shot timer */
void oneshot_callback(int timer_id)
{
    oneshot_fired = 1;
    uart_puts("One-shot callback fired!\n");
}

int main()
{
    uart_puts("=== Timer Library Test ===\n\n");
    
    /* Initialize timer subsystem */
    timer_init();

    /* Test 1: Basic one-shot timer with callback */
    uart_puts("Test 1: One-shot timer with callback (300ms)...\n");
    timer_set_callback(TIMER_0, oneshot_callback);
    timer_start_ms(TIMER_0, 300);
    
    while (!oneshot_fired) {
        /* Wait for callback */
    }
    uart_puts("Test 1 passed!\n\n");

    /* Test 2: Periodic timer - 5 ticks at 100ms each */
    uart_puts("Test 2: Periodic timer (100ms x 5 ticks)...\n");
    periodic_count = 0;
    timer_set_callback(TIMER_1, periodic_callback);
    timer_start_periodic(TIMER_1, 100);
    
    while (periodic_count < 5) {
        /* Wait for 5 ticks */
    }
    
    /* Cancel periodic timer */
    timer_cancel(TIMER_1);
    uart_puts("Periodic timer cancelled.\n");
    uart_puts("Test 2 passed!\n\n");

    /* Test 3: delay() function */
    uart_puts("Test 3: delay() function...\n");
    uart_puts("Delaying 200ms...");
    delay(200);
    uart_puts(" done!\n");
    uart_puts("Three quick delays: ");
    delay(100);
    uart_puts("1.");
    delay(100);
    uart_puts("2.");
    delay(100);
    uart_puts("3!\n");
    uart_puts("Test 3 passed!\n\n");

    /* Test 4: Check timer_is_active */
    uart_puts("Test 4: timer_is_active() check...\n");
    uart_puts("Timer 0 active: ");
    uart_putint(timer_is_active(TIMER_0));
    uart_puts(" (expected 0)\n");
    
    timer_start_periodic(TIMER_0, 500);
    uart_puts("Timer 0 active after start_periodic: ");
    uart_putint(timer_is_active(TIMER_0));
    uart_puts(" (expected 1)\n");
    
    timer_cancel(TIMER_0);
    uart_puts("Timer 0 active after cancel: ");
    uart_putint(timer_is_active(TIMER_0));
    uart_puts(" (expected 0)\n");
    uart_puts("Test 4 passed!\n\n");

    uart_puts("=== All Timer Tests Passed! ===\n");
    return 0;
}

void interrupt()
{
    int int_id = get_int_id();
    switch (int_id)
    {
        case INTID_TIMER0:
            timer_isr_handler(TIMER_0);
            break;
        case INTID_TIMER1:
            timer_isr_handler(TIMER_1);
            break;
        case INTID_TIMER2:
            timer_isr_handler(TIMER_2);
            break;
        default:
            break;
    }
}
