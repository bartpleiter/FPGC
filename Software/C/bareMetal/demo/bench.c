#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_TERM
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"


#define N 256
#define LEN 854  // (10*N) / 3 + 1

int frameCount = 0;

void spigotPiBench()
{
  int a[LEN];
  int z;
  for (z=0; z<LEN; z++) 
  {
    a[z] = 0;
  }
  frameCount = 0;
  while (frameCount == 0); // wait until next frame to start
  frameCount = 0;

  int j = 0;
  int predigit = 0;
  int nines = 0;
  int x = 0;
  int q = 0;
  int k = 0;
  int len = 0;
  int i = 0;
  int y = 0;

  for(j=N; j; ) 
  {
    q = 0;
    k = LEN+LEN-1;

    for(i=LEN; i; --i) 
    {
      if (j == N)
      {
        x = 20 + q*i;
      }
      else
      {
        x = (10*a[i-1]) + q*i;
      }
      q = x / k;
      a[i-1] = (x-q*k);
      k -= 2;
    }

    k = x % 10;

    if (k==9)
    {
      ++nines;
    }

    else 
    {
      if (j)
      {
        --j;
        y = predigit + (x / 10);
        term_putint(y);
      }

      for(; nines; --nines)
      {
        if (j)
        {
          --j;
          if (x >= 10)
          {
            term_puts("0");
          }
          else
          {
            term_puts("9");
          }
        }
      }

      predigit = k;
    }
  }

  term_puts("\nPiBench256 took    ");
  term_putint(frameCount);
  term_puts(" frames\n");
}

// LoopBench: a simple increase and loop bench for a set amount of time.
// reads framecount from memory in the loop.
// no pipeline clears within the loop.
int loopBench()
{
  int retval = 0;
  asm(
    "push r1"
    "push r2"
    "push r3"
    "push r4"

    "addr2reg Label_frameCount r2"
    "write 0 r2 r0 ; reset frameCount"
    "load 0 r4 ; score"

    "Label_ASM_Loop:"
        "read 0 r2 r3 ; read frameCount"
        "slt r3 300 r3 ; TESTDURATION here in frames"
        "beq r3 r0 Label_ASM_Done ; check if done "
        "add r4 1 r4 ; increase score and loop"
        "jump Label_ASM_Loop"


    "Label_ASM_Done:"
        "write -1 r14 r4 ; set return value"
    
        "pop r4"
    "pop r3"
    "pop r2"
    "pop r1"
      );

  return retval;
}

// CountMillionBench: how many frames it takes to do a C for loop to a million
int countMillionBench()
{
  frameCount = 0;
  while (frameCount == 0); // wait until next frame to start
  frameCount = 0;
  int i;
  for (i = 0; i < 1000000; i++);
  return frameCount;
}

int main() {
  // Reset GPU VRAM
  gpu_clear_vram();

  // Load default pattern and palette tables
  unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
  gpu_load_pattern_table(pattern_table + 3); // +3 to skip function prologue

  unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
  gpu_load_palette_table(palette_table + 3); // +3 to skip function prologue

  // Initialize terminal
  term_init();

  term_puts("----------------FPGCbench---------------");

  term_puts("LoopBench:         ");
  frameCount = 0;
  while(!frameCount); // Wait until next frame to start
  int score = loopBench();
  term_putint(score);
  term_puts("\n");

  term_puts("\nCountMillionBench: ");
  score = countMillionBench();
  term_putint(score);
  term_puts(" frames\n");

  term_puts("\nPiBench256:\n");
  spigotPiBench();

  return 0x39;
}

void interrupt()
{
  int int_id = get_int_id();
  switch (int_id)
  {
    case INTID_FRAME_DRAWN:
      frameCount++;
      break;
    default:
      break;
  }
}
