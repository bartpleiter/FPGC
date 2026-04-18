/* hello.c — minimal smoke test for the on-device modern-C toolchain.
 *
 * Build (on the FPGC):
 *   cc /user/hello.c hello
 * Run:
 *   hello
 */
#include <syscall.h>

int main(void)
{
    sys_putstr("Hello from a self-hosted compile!\n");
    return 0;
}
