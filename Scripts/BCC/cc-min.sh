#!/bin/sh
# /bin/cc-min — minimal cc for validating the on-device toolchain end-to-end.
#
# Usage:   cc-min <input.c> <output_name>
#
# Compiles ONLY the user source plus the userlib syscall wrappers — enough
# for programs that use sys_putstr / sys_putc / sys_exit etc. directly.
# No stdio, malloc, string, ctype, time, etc. Use plain `cc` for those.
#
# Useful for quickly testing that cpp/cproc/qbe/asm-link all work on device
# without paying the full libc compile cost on every iteration.
set -e

cpp -I /lib/include /lib/src/syscall.c -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/syscall.asm || true

cpp -I /lib/include "$1" -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/user.asm || true

asm-link -o /bin/$2 /lib/asm/crt0_ubdos.asm /lib/asm/syscall_asm.asm /tmp/syscall.asm /tmp/user.asm

echo "cc-min: built /bin/$2"
