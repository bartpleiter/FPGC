#include <stdio.h>
static int   nums[64];
static char  buf[256];
int main(void) {
    int i, sum = 0;
    for (i = 0; i < 64;  i++) sum += nums[i];
    for (i = 0; i < 256; i++) sum += buf[i];
    printf("bss sum = %d (expect 0)\n", sum);
    return 0;
}
