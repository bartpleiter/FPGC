// Benchmark test program ported from the FPGC6 to compare performance
// Bare metal version as BDOS is not available yet as of writing
// Very messy code as the goal is to quickly get the numbers, a refactor will be done as UserBDOS program
#include "libs/kernel/gfx.c"
#include "libs/kernel/gpu_data_ascii.c"

#define N    256  // Decimals of pi to compute.
#define LEN  854  // (10*N) / 3 + 1

char *a = (char*) 0x440000;

int frameCount = 0;

/*
Recursive helper function for itoa
Eventually returns the number of digits in n
s is the output buffer
*/
int itoar(int n, char *s)
{
    int digit = n % 10;
    int i = 0;

    n = n / 10;
    if ((unsigned int) n > 0)
        i += itoar(n, s);

    s[i++] = digit + '0';

    return i;
}


/*
Converts integer n to characters.
The characters are placed in the buffer s.
The buffer is terminated with a 0 value.
Uses recursion, division and mod to compute.
*/
void itoa(int n, char *s)
{
    // compute and fill the buffer
    int i = itoar(n, s);

    // end with terminator
    s[i] = 0;
}

void spigotPiBench()
{
  frameCount = 0;
  while (frameCount == 0); // wait until next frame to start
  frameCount = 0;

  char charbuf[12];

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
        itoa(y, charbuf);
        GFX_puts(charbuf);
      }

      for(; nines; --nines)
      {
        if (j)
        {
          --j;
          if (x >= 10)
          {
            GFX_puts("0");
          }
          else
          {
            GFX_puts("9");
          }
        }
      }

      predigit = k;
    }
  }

  itoa(frameCount, charbuf);
  GFX_puts("\nPiBench256 took    ");
  GFX_puts(charbuf);
  GFX_puts(" frames\n");
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
    GFX_init();

    // Copy the ASCII pattern table to VRAM
    unsigned int* pattern_table = (unsigned int*)&DATA_ASCII_DEFAULT;
    GFX_copy_pattern_table(pattern_table + 3); // +3 to skip function prologue

    // Copy the palette table to VRAM
    unsigned int* palette_table = (unsigned int*)&DATA_PALETTE_DEFAULT;
    GFX_copy_palette_table(palette_table + 3); // +3 to skip function prologue

    GFX_puts("----------------FPGCbench---------------");

    GFX_puts("LoopBench:         ");
    frameCount = 0;
    while(!frameCount); // Wait until next frame to start
    int score = loopBench();
    char scoreStr[12];
    itoa(score, scoreStr);
    GFX_puts(scoreStr);
    GFX_puts("\n");

    GFX_puts("\nCountMillionBench: ");
    score = countMillionBench();
    itoa(score, scoreStr);
    GFX_puts(scoreStr);
    GFX_puts(" frames\n");

    GFX_puts("\nPiBench256:\n");
    spigotPiBench();

    return 0x39;
}

void interrupt()
{
    frameCount++;
}
