#ifndef TIME_C
#define TIME_C

//
// time library implementation.
//

#include "libs/common/time.h"

// Return the current microsecond counter value.
unsigned int get_micros()
{
  unsigned int *micros_ptr = (unsigned int *)MICROS_ADDR;
  return *micros_ptr;
}

#endif // TIME_C
