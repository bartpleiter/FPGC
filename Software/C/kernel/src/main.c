/*
 * main.c — BDOS v4 kernel entry point, interrupt dispatcher, and main loop.
 *
 * Boot: main() → kernel_init() → kernel_loop()
 * Main loop: HID polling, network polling, scheduler tick, shell tick
 * Interrupts: dispatched by interrupt() via get_int_id()
 */
#include "kernel.h"

/* Assembly helpers (from crt0_kernel.asm) */
extern void kernel_loop_save_sp_bp(void);

void kernel_panic(const char *msg)
{
    term_set_palette(PALETTE_WHITE_ON_RED);
    term_puts("KERNEL PANIC: ");
    term_puts(msg);
    term_puts("\nSystem halted.\n");

    uart_puts("KERNEL PANIC: ");
    uart_puts(msg);
    uart_puts("\nSystem halted.\n");

    kernel_halt();
}

void kernel_log(const char *msg)
{
    term_puts(msg);
}

void kernel_loop(void)
{
    kernel_loop_save_sp_bp();

    while (1)
    {
        hid_poll();
        net_poll();
        fnp_poll();
        sched_tick();
    }
}

int main(void)
{
    kernel_init();

    /* Spawn /bin/init as PID 1 */
    {
        int pid;
        pid = proc_spawn("/bin/init", 0, (char **)0);
        if (pid < 0)
            kernel_panic("failed to spawn /bin/init");
        sched_should_yield = 1;
    }

    kernel_loop();
    return 0x42;
}

void interrupt(void)
{
    int int_id;

    int_id = get_int_id();
    switch (int_id)
    {
    case FPGC_INTID_UART:
        /* UART RX — could buffer incoming bytes */
        break;

    case FPGC_INTID_TIMER0:
        /* Timer 0: deferred ENC28J60 ISR retry OR scheduler tick */
        if (net_isr_deferred)
        {
            if (enc28j60_spi_in_use)
            {
                timer_set(TIMER_0, 1);
                timer_start(TIMER_0);
            }
            else
            {
                net_isr_deferred = 0;
                net_isr_drain();
            }
        }
        break;

    case FPGC_INTID_TIMER1:
        /* Timer 1: USB keyboard HID report polling (10ms) */
        timer_isr_handler(TIMER_1);
        break;

    case FPGC_INTID_TIMER2:
        /* Timer 2: delay() completion */
        timer_isr_handler(TIMER_2);
        break;

    case FPGC_INTID_FRAME_DRAWN:
        break;

    case FPGC_INTID_ETH:
        /* ENC28J60 RX interrupt */
        if (enc28j60_spi_in_use)
        {
            net_isr_deferred = 1;
            timer_set(TIMER_0, 1);
            timer_start(TIMER_0);
        }
        else
        {
            net_isr_drain();
        }
        break;

    case FPGC_INTID_DMA:
        /* DMA complete — could signal waiting processes */
        break;

    default:
        break;
    }

    /* Check for multitasking switch/kill requests (reuse v3 mechanism
     * for now — will be replaced by proper scheduler in later commits) */
    /* TODO: implement proper scheduler-driven context switch */
}
