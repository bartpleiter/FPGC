// Simple test program that runs for a long time to allow testing of suspend/resume and multitasking features.

#define USER_SYSCALL
#include "libs/user/user.h"

int main()
{
  int i = 0;
  int val = 0;

  sys_print_str("Test start!\n");
  sys_print_str("Counting..\n");

  while (1)
  {
    // Print a repeating message without using timers for now
    if (i % 1000000 == 0)
    {
      val = val + 1;
      sys_print_str("Loop iteration: ");
      sys_print_char('0' + (val % 10));
      sys_print_char('\n');
    }
    i++;
  }

  return 37;
}
