#!/bin/sh
# /bin/libc-build — compile every libc + userlib .c source in /lib/src/
# to a cached .asm in /lib/asm-cache/. Run once (or after editing libc)
# so subsequent `cc` invocations only have to compile the user source.
#
# Pipeline per file:
#   cpp    : preprocess (handles #include via /lib/include)
#   cproc  : C  -> QBE IR
#   qbe    : QBE IR -> b32p3 assembly
set -e

mkdir /lib/asm-cache || true

cpp -I /lib/include /lib/src/string.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/string.asm || true

cpp -I /lib/include /lib/src/stdlib.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/stdlib.asm || true

cpp -I /lib/include /lib/src/malloc.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/malloc.asm || true

cpp -I /lib/include /lib/src/ctype.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/ctype.asm || true

cpp -I /lib/include /lib/src/stdio.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/stdio.asm || true

cpp -I /lib/include /lib/src/syscall.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/syscall.asm || true

cpp -I /lib/include /lib/src/io_stubs.c  -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/io_stubs.asm || true

cpp -I /lib/include /lib/src/time.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/time.asm || true

cpp -I /lib/include /lib/src/fixedmath.c -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fixedmath.asm || true

cpp -I /lib/include /lib/src/fixed64.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fixed64.asm || true

cpp -I /lib/include /lib/src/plot.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/plot.asm || true

cpp -I /lib/include /lib/src/fnp.c       -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fnp.asm || true

echo "libc-build: all libc + userlib sources compiled to /lib/asm-cache/"
