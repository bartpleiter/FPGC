#!/bin/sh
# /bin/cc — compile a C file to a BDOS executable, on-device, from sources.
#
# Usage:   cc <input.c> <output_name>
# Result:  /bin/<output_name>  (relocatable userBDOS binary)
#
# Pipeline (each stage is a separate userBDOS program):
#   cpp     : preprocess source (handles #include, #define, #ifdef)
#   cproc   : C -> QBE IR (target b32p3)
#   qbe     : QBE IR -> b32p3 assembly
#   asm-link: assemble and link everything together
#
# All libc + userlib C sources live in /lib/src/ and are recompiled fresh on
# every `cc` invocation. Hand-written assembly (crt0, syscall stubs, fixed64
# helpers) lives in /lib/asm/ and is fed straight to the linker.
#
# This is intentionally slow but matches the self-hosting goal: the device
# bootstraps everything itself from text sources.
set -e

cpp -I /lib/include /lib/src/string.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/string.asm || true

cpp -I /lib/include /lib/src/stdlib.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/stdlib.asm || true

cpp -I /lib/include /lib/src/malloc.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/malloc.asm || true

cpp -I /lib/include /lib/src/ctype.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/ctype.asm || true

cpp -I /lib/include /lib/src/stdio.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/stdio.asm || true

cpp -I /lib/include /lib/src/syscall.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/syscall.asm || true

cpp -I /lib/include /lib/src/io_stubs.c  -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/io_stubs.asm || true

cpp -I /lib/include /lib/src/time.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/time.asm || true

cpp -I /lib/include /lib/src/fixedmath.c -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/fixedmath.asm || true

cpp -I /lib/include /lib/src/fixed64.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/fixed64.asm || true

cpp -I /lib/include /lib/src/plot.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/plot.asm || true

cpp -I /lib/include /lib/src/fnp.c       -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/fnp.asm || true

cpp -I /lib/include "$1" -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /tmp/user.asm || true

asm-link -o /bin/$2 /lib/asm/crt0_ubdos.asm /tmp/string.asm /tmp/stdlib.asm /tmp/malloc.asm /tmp/ctype.asm /tmp/stdio.asm /lib/asm/syscall_asm.asm /tmp/syscall.asm /tmp/io_stubs.asm /tmp/time.asm /tmp/fixedmath.asm /lib/asm/fixed64_asm.asm /tmp/fixed64.asm /tmp/plot.asm /tmp/fnp.asm /tmp/user.asm

echo "cc: built /bin/$2"
