// Test: __builtin_divfp with function call arguments
// divfp(func(), func()) must preserve first call result across second call

int int2fixed_fn(int x)
{
  return x << 16;
}

int main(void)
{
  // divfp(6.0, 2.0) in Q16.16 = 3.0 => 0x30000 >> 16 = 3
  int r = __builtin_divfp(int2fixed_fn(6), int2fixed_fn(2));
  return (r >> 16) + 4; // expected=0x07
}

void interrupt(void) {}
