#include "libs/kernel/sys.h"

int get_int_id()
{
    int retval = 0;
    asm(
        "readintid r11      ; r11 = interrupt ID"
        "write -1 r14 r11   ; Write to stack for return"
    );
    return retval;
}
