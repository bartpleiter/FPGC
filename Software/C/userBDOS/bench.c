// bench.c — FPGCbench for BDOS
// Ports the bare-metal benchmark suite to run as a userBDOS program.
// Uses the hardware microsecond counter (memory-mapped at 0x1C000068) for
// timing instead of the 60 Hz FRAME_DRAWN interrupt.

#define USER_SYSCALL
#define COMMON_STDLIB
#define COMMON_STRING
#define COMMON_TIME
#include "libs/user/user.h"
#include "libs/common/common.h"

// ---------- helpers ----------

void print_int(int n)
{
  char buf[16];
  itoa(n, buf, 10);
  sys_print_str(buf);
}

void print_uint(unsigned int n)
{
  char buf[16];
  utoa(n, buf, 10, 0);
  sys_print_str(buf);
}

// ---------- PiBench256 ----------

#define N 256
#define LEN 854  // (10*N) / 3 + 1

void spigotPiBench()
{
  int a[LEN];
  int z;
  for (z = 0; z < LEN; z++)
  {
    a[z] = 0;
  }

  unsigned int start_us = get_micros();

  int j = 0;
  int predigit = 0;
  int nines = 0;
  int x = 0;
  int q = 0;
  int k = 0;
  int len = 0;
  int i = 0;
  int y = 0;

  for (j = N; j; )
  {
    q = 0;
    k = LEN + LEN - 1;

    for (i = LEN; i; --i)
    {
      if (j == N)
      {
        x = 20 + q * i;
      }
      else
      {
        x = (10 * a[i - 1]) + q * i;
      }
      q = x / k;
      a[i - 1] = (x - q * k);
      k -= 2;
    }

    k = x % 10;

    if (k == 9)
    {
      ++nines;
    }
    else
    {
      if (j)
      {
        --j;
        y = predigit + (x / 10);
        print_int(y);
      }

      for (; nines; --nines)
      {
        if (j)
        {
          --j;
          if (x >= 10)
          {
            sys_print_str("0");
          }
          else
          {
            sys_print_str("9");
          }
        }
      }

      predigit = k;
    }
  }

  unsigned int elapsed_us = get_micros() - start_us;
  unsigned int elapsed_ms = elapsed_us / 1000;

  sys_print_str("\nPiBench256 took    ");
  print_uint(elapsed_ms);
  sys_print_str(" ms\n");
}

// ---------- LoopBench ----------
// Tight loop for 5 seconds (5 000 000 us), counting iterations.
// The main loop has the same instruction count and branch structure as the
// original frame-based version: read, slt, beq(not taken), add, jump(taken)
// = 5 instructions per iteration with 1 taken jump.

int loopBench()
{
  int retval = 0;
  asm(
    "push r1"
    "push r2"
    "push r3"
    "push r4"
    "push r5"

    "load32 0x1C000068 r2       ; r2 = micros register address"
    "read 0 r2 r3              ; r3 = start micros"
    "load32 5000000 r5         ; r5 = duration in microseconds (5 s)"
    "add r3 r5 r5              ; r5 = end micros (start + duration)"
    "load 0 r4                 ; r4 = score (iteration count)"

    "Label_ASM_Loop:"
        "read 0 r2 r3          ; r3 = current micros"
        "slt r3 r5 r3          ; r3 = (current < end) ? 1 : 0"
        "beq r3 r0 Label_ASM_Done ; if done (r3==0), branch out"
        "add r4 1 r4           ; increment score"
        "jump Label_ASM_Loop   ; loop"

    "Label_ASM_Done:"
        "write -4 r14 r4       ; set return value"

    "pop r5"
    "pop r4"
    "pop r3"
    "pop r2"
    "pop r1"
  );

  return retval;
}

// ---------- CountMillionBench ----------
// Measures how long a C for-loop to one million takes, in microseconds.

int countMillionBench()
{
  unsigned int start_us = get_micros();
  int i;
  for (i = 0; i < 1000000; i++);
  unsigned int elapsed_us = get_micros() - start_us;
  return elapsed_us;
}

int main()
{
  sys_print_str("----------------FPGCbench---------------\n");

  sys_print_str("LoopBench (5 s):   ");
  int score = loopBench();
  print_int(score);
  sys_print_str(" iterations\n");

  sys_print_str("\nCountMillionBench: ");
  unsigned int us = countMillionBench();
  print_uint(us / 1000);
  sys_print_str(" ms (");
  print_uint(us);
  sys_print_str(" us)\n");

  sys_print_str("\nPiBench256:\n");
  spigotPiBench();

  return 0;
}

void interrupt()
{
  // No interrupts used — timing is via the hardware micros counter.
}
