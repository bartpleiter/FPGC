// Syscall test program for userBDOS.

#define USER_SYSCALL
#include "libs/user/user.h"

int main()
{
  // Test 1: print a string
  sys_print_str("Hello from userBDOS syscall!\n");

  // Test 2: print individual characters
  sys_print_char('A');
  sys_print_char('B');
  sys_print_char('C');
  sys_print_char('\n');

  // Test 3: print another string
  sys_print_str("Syscall test complete.\n");

  return 0;
}
