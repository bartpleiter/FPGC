#!/bin/sh
# /bin/cc — compile a C file to a BDOS executable, on-device.
#
# Usage:   cc <input.c> <output_name>
# Result:  /bin/<output_name>  (relocatable userBDOS binary)
#
# Pipeline:
#   cpp     : preprocess source (handles #include, #define, #ifdef)
#   cproc   : C -> QBE IR (target b32p3)
#   qbe     : QBE IR -> b32p3 assembly
#   asm-link: assemble and link everything together
#
# Libc + userlib are NOT recompiled per invocation. They live as cached
# .asm files in /lib/asm-cache/ — run /bin/libc-build once (after first
# sync, or after editing a /lib/src/*.c) to populate the cache. Hand-
# written assembly (crt0, syscall stubs, fixed64 helpers) is in /lib/asm/.
set -e

cpp -I /lib/include "$1" -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/user.asm

asm-link -o /bin/$2 /lib/asm/crt0_ubdos.asm /lib/asm-cache/string.asm /lib/asm-cache/stdlib.asm /lib/asm-cache/malloc.asm /lib/asm-cache/ctype.asm /lib/asm-cache/stdio.asm /lib/asm/syscall_asm.asm /lib/asm-cache/syscall.asm /lib/asm-cache/io_stubs.asm /lib/asm-cache/time.asm /lib/asm-cache/fixedmath.asm /lib/asm/fixed64_asm.asm /lib/asm-cache/fixed64.asm /lib/asm-cache/plot.asm /lib/asm-cache/fnp.asm /tmp/user.asm

echo "cc: built /bin/$2"
