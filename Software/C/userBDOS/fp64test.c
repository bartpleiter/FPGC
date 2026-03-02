// fp64test.c - FP64 Coprocessor Hardware Validation Program
// Runs on BDOS. Tests all new FP64 coprocessor instructions on real hardware.
// Each test returns 1=PASS, 0=FAIL. Results are printed to terminal.

#define USER_SYSCALL
#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/user/user.h"
#include "libs/common/common.h"

// ---- helpers ----

void print_hex(unsigned int n)
{
  char buf[16];
  utoa(n, buf, 16, 1);
  sys_print_str("0x");
  sys_print_str(buf);
}

void print_int_val(int n)
{
  char buf[16];
  itoa(n, buf, 10);
  sys_print_str(buf);
}

void print_test(char *name, int pass)
{
  sys_print_str("  ");
  sys_print_str(name);
  if (pass) sys_print_str(": PASS\n");
  else sys_print_str(": *** FAIL ***\n");
}

// ---- Test 1: FLD + FSTHI + FSTLO roundtrip ----
// Load {0x12345678, 0xABCDEF01} into f0, read back hi and lo
int test_fld_roundtrip()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3" "push r4"

    "load32 0x12345678 r1"
    "load32 0xABCDEF01 r2"
    "fld r1 r2 r0"

    "fsthi r0 r0 r3"
    "fstlo r0 r0 r4"

    "load32 0x12345678 r1"
    "bne r3 r1 _t1_fail"
    "load32 0xABCDEF01 r1"
    "bne r4 r1 _t1_fail"
    "load 1 r1"
    "jump _t1_end"
    "_t1_fail:"
    "load 0 r1"
    "_t1_end:"
    "write -1 r14 r1"

    "pop r4" "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 2: FADD basic: {0,5} + {0,3} = {0,8} ----
int test_fadd_basic()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load 0 r1"
    "load 5 r2"
    "fld r1 r2 r0"
    "load 3 r2"
    "fld r1 r2 r1"

    "fadd r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "bne r3 r0 _t2_fail"
    "load 8 r2"
    "bne r1 r2 _t2_fail"
    "load 1 r1"
    "jump _t2_end"
    "_t2_fail:"
    "load 0 r1"
    "_t2_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 3: FADD carry: {0,0xFFFFFFFF} + {0,1} = {1,0} ----
int test_fadd_carry()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load 0 r1"
    "load32 0xFFFFFFFF r2"
    "fld r1 r2 r0"
    "load 1 r2"
    "fld r1 r2 r1"

    "fadd r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "load 1 r2"
    "bne r3 r2 _t3_fail"
    "bne r1 r0 _t3_fail"
    "load 1 r1"
    "jump _t3_end"
    "_t3_fail:"
    "load 0 r1"
    "_t3_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 4: FSUB: {1,0} - {0,5} = {0, 0xFFFFFFFB} ----
int test_fsub()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load 1 r1"
    "load 0 r2"
    "fld r1 r2 r0"
    "load 0 r1"
    "load 5 r2"
    "fld r1 r2 r1"

    "fsub r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "bne r3 r0 _t4_fail"
    "load32 0xFFFFFFFB r2"
    "bne r1 r2 _t4_fail"
    "load 1 r1"
    "jump _t4_end"
    "_t4_fail:"
    "load 0 r1"
    "_t4_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 5: FMUL 1.0 x 1.0 = 1.0 ----
// {1,0} x {1,0} = {1,0}
int test_fmul_one()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load 1 r1"
    "load 0 r2"
    "fld r1 r2 r0"
    "fld r1 r2 r1"

    "fmul r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "load 1 r2"
    "bne r3 r2 _t5_fail"
    "bne r1 r0 _t5_fail"
    "load 1 r1"
    "jump _t5_end"
    "_t5_fail:"
    "load 0 r1"
    "_t5_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 6: FMUL 2.5 x 2.0 = 5.0 ----
// 2.5 = {2, 0x80000000}, 2.0 = {2, 0}, result = {5, 0}
int test_fmul_frac()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load 2 r1"
    "load32 0x80000000 r2"
    "fld r1 r2 r0"
    "load 2 r1"
    "load 0 r2"
    "fld r1 r2 r1"

    "fmul r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "load 5 r2"
    "bne r3 r2 _t6_fail"
    "bne r1 r0 _t6_fail"
    "load 1 r1"
    "jump _t6_end"
    "_t6_fail:"
    "load 0 r1"
    "_t6_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 7: FMUL negative: (-1.0) x 2.0 = -2.0 ----
// -1.0 = {0xFFFFFFFF, 0}, 2.0 = {2, 0}, result = {0xFFFFFFFE, 0}
int test_fmul_neg()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load32 0xFFFFFFFF r1"
    "load 0 r2"
    "fld r1 r2 r0"
    "load 2 r1"
    "load 0 r2"
    "fld r1 r2 r1"

    "fmul r0 r1 r2"

    "fsthi r2 r0 r3"
    "fstlo r2 r0 r1"

    "load32 0xFFFFFFFE r2"
    "bne r3 r2 _t7_fail"
    "bne r1 r0 _t7_fail"
    "load 1 r1"
    "jump _t7_end"
    "_t7_fail:"
    "load 0 r1"
    "_t7_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 8: MULSHI: 0x10000 x 0x30000 -> high = 3 ----
int test_mulshi()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load32 0x10000 r1"
    "load32 0x30000 r2"
    "mulshi r1 r2 r3"

    "load 3 r1"
    "bne r3 r1 _t8_fail"
    "load 1 r1"
    "jump _t8_end"
    "_t8_fail:"
    "load 0 r1"
    "_t8_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 9: MULTUHI: 0x80000000 x 2 -> high = 1 ----
int test_multuhi()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    "load32 0x80000000 r1"
    "load 2 r2"
    "multuhi r1 r2 r3"

    "load 1 r1"
    "bne r3 r1 _t9_fail"
    "load 1 r1"
    "jump _t9_end"
    "_t9_fail:"
    "load 0 r1"
    "_t9_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 10: FMUL then FADD: 3*4+1 = 13 ----
int test_fmul_then_fadd()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3"

    // f0 = 3.0
    "load 3 r1"
    "load 0 r2"
    "fld r1 r2 r0"
    // f1 = 4.0
    "load 4 r1"
    "fld r1 r2 r1"
    // f2 = 1.0
    "load 1 r1"
    "fld r1 r2 r2"

    // f3 = f0 * f1 = 12.0
    "fmul r0 r1 r3"
    // f4 = f3 + f2 = 13.0
    "fadd r3 r2 r4"

    "fsthi r4 r0 r3"

    "load 13 r1"
    "bne r3 r1 _t10_fail"
    "load 1 r1"
    "jump _t10_end"
    "_t10_fail:"
    "load 0 r1"
    "_t10_end:"
    "write -1 r14 r1"

    "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- Test 11: FP register isolation ----
// Load different values into f0-f3, verify no cross-contamination
int test_fp_reg_isolation()
{
  int result = 0;
  asm(
    "push r1" "push r2" "push r3" "push r4"

    "load 0 r2"
    "load 10 r1"
    "fld r1 r2 r0"
    "load 20 r1"
    "fld r1 r2 r1"
    "load 30 r1"
    "fld r1 r2 r2"
    "load 40 r1"
    "fld r1 r2 r3"

    // Read back and verify each one
    "fsthi r0 r0 r1"
    "load 10 r2"
    "bne r1 r2 _t11_fail"

    "fsthi r1 r0 r1"
    "load 20 r2"
    "bne r1 r2 _t11_fail"

    "fsthi r2 r0 r1"
    "load 30 r2"
    "bne r1 r2 _t11_fail"

    "fsthi r3 r0 r1"
    "load 40 r2"
    "bne r1 r2 _t11_fail"

    "load 1 r1"
    "jump _t11_end"
    "_t11_fail:"
    "load 0 r1"
    "_t11_end:"
    "write -1 r14 r1"

    "pop r4" "pop r3" "pop r2" "pop r1"
  );
  return result;
}

// ---- main ----

int main()
{
  int passed = 0;
  int total = 11;

  sys_print_str("=== FP64 Coprocessor Hardware Validation ===\n\n");

  int r;
  r = test_fld_roundtrip();      print_test("FLD/FSTHI/FSTLO roundtrip", r); passed = passed + r;
  r = test_fadd_basic();          print_test("FADD basic {0,5}+{0,3}", r);   passed = passed + r;
  r = test_fadd_carry();          print_test("FADD carry {0,FF..F}+{0,1}", r); passed = passed + r;
  r = test_fsub();                print_test("FSUB {1,0}-{0,5}", r);          passed = passed + r;
  r = test_fmul_one();            print_test("FMUL 1.0 x 1.0", r);            passed = passed + r;
  r = test_fmul_frac();           print_test("FMUL 2.5 x 2.0", r);            passed = passed + r;
  r = test_fmul_neg();            print_test("FMUL -1.0 x 2.0", r);           passed = passed + r;
  r = test_mulshi();              print_test("MULSHI 0x10000*0x30000", r);     passed = passed + r;
  r = test_multuhi();             print_test("MULTUHI 0x80000000*2", r);       passed = passed + r;
  r = test_fmul_then_fadd();      print_test("FMUL+FADD 3*4+1=13", r);        passed = passed + r;
  r = test_fp_reg_isolation();    print_test("FP reg isolation", r);           passed = passed + r;

  sys_print_str("\n");
  print_int_val(passed);
  sys_print_str("/");
  print_int_val(total);
  sys_print_str(" tests passed");

  if (passed == total)
    sys_print_str(" - ALL OK!\n");
  else
    sys_print_str(" - FAILURES DETECTED\n");

  return 0;
}

void interrupt()
{
}
