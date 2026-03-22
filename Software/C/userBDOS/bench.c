// bench.c — FPGCbench for BDOS
// Ports the bare-metal benchmark suite to run as a userBDOS program.
// Uses the hardware microsecond counter for timing.

#include <syscall.h>
#include <time.h>

// ---------- helpers ----------

void print_int(int n)
{
  char buf[16];
  int i = 0;
  int j;
  char tmp[12];
  int neg = 0;

  if (n < 0)
  {
    neg = 1;
    n = -n;
  }
  if (n == 0)
  {
    tmp[i++] = '0';
  }
  else
  {
    while (n > 0)
    {
      tmp[i++] = '0' + (n % 10);
      n = n / 10;
    }
  }
  j = 0;
  if (neg)
  {
    buf[j++] = '-';
  }
  while (i > 0)
  {
    buf[j++] = tmp[--i];
  }
  buf[j] = 0;
  sys_print_str(buf);
}

void print_uint(unsigned int n)
{
  char buf[16];
  char tmp[12];
  int i = 0;
  int j;

  if (n == 0)
  {
    tmp[i++] = '0';
  }
  else
  {
    while (n > 0)
    {
      tmp[i++] = '0' + (n % 10);
      n = n / 10;
    }
  }
  j = 0;
  while (i > 0)
  {
    buf[j++] = tmp[--i];
  }
  buf[j] = 0;
  sys_print_str(buf);
}

// ---------- PiBench256 ----------

#define N 256
#define LEN 854  // (10*N) / 3 + 1

void spigotPiBench(void)
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
// Tight C loop for 5 seconds (5,000,000 us), counting iterations.
// Note: This is a pure C version — results will differ from the original
// B32CC inline-asm version that had a fixed 5-instruction loop body.

int loopBench(void)
{
  unsigned int end;
  int count = 0;

  end = get_micros() + 5000000;
  while (get_micros() < end)
  {
    count++;
  }

  return count;
}

// ---------- CountMillionBench ----------
// Measures how long a C for-loop to one million takes, in microseconds.

int countMillionBench(void)
{
  unsigned int start_us = get_micros();
  int i;
  for (i = 0; i < 1000000; i++)
    ;
  unsigned int elapsed_us = get_micros() - start_us;
  return elapsed_us;
}

int main(void)
{
  int score;
  unsigned int us;

  sys_print_str("----------------FPGCbench---------------\n");

  sys_print_str("LoopBench (5 s):   ");
  score = loopBench();
  print_int(score);
  sys_print_str(" iterations\n");

  sys_print_str("\nCountMillionBench: ");
  us = countMillionBench();
  print_uint(us / 1000);
  sys_print_str(" ms (");
  print_uint(us);
  sys_print_str(" us)\n");

  sys_print_str("\nPiBench256:\n");
  spigotPiBench();

  return 0;
}
