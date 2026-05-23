#!/bin/sh
# /bin/libc-build — compile every libc + userlib .c source in /lib/src/
# to a cached .asm in /lib/asm-cache/. Run once (or after editing libc)
# so subsequent `cc` invocations only have to compile the user source.
#
# Pipeline per file:
#   cpp    : preprocess (handles #include via /lib/include)
#   cproc  : C  -> QBE IR
#   qbe    : QBE IR -> b32p3 assembly
#
# BDOS v4: scripts abort automatically on any non-zero exit code.

echo "libc-build: compiling 13 library sources..."

mkdir -p /lib/asm-cache

echo "[1/13] string.c"
cpp -I /lib/include /lib/src/string.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/string.asm

echo "[2/13] stdlib.c"
cpp -I /lib/include /lib/src/stdlib.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/stdlib.asm

echo "[3/13] malloc.c"
cpp -I /lib/include /lib/src/malloc.c    -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/malloc.asm

echo "[4/13] ctype.c"
cpp -I /lib/include /lib/src/ctype.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/ctype.asm

echo "[5/13] stdio.c"
cpp -I /lib/include /lib/src/stdio.c     -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/stdio.asm

echo "[6/13] syscall.c"
cpp -I /lib/include /lib/src/syscall.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/syscall.asm

echo "[7/13] io_stubs.c"
cpp -I /lib/include /lib/src/io_stubs.c  -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/io_stubs.asm

echo "[8/13] time.c"
cpp -I /lib/include /lib/src/time.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/time.asm

echo "[9/13] fixedmath.c"
cpp -I /lib/include /lib/src/fixedmath.c -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fixedmath.asm

echo "[10/13] fixed64.c"
cpp -I /lib/include /lib/src/fixed64.c   -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fixed64.asm

echo "[11/13] plot.c"
cpp -I /lib/include /lib/src/plot.c      -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/plot.asm

echo "[12/13] fnp.c"
cpp -I /lib/include /lib/src/fnp.c       -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/fnp.asm

echo "[13/13] dma.c"
cpp -I /lib/include /lib/src/dma.c       -o /tmp/c.i
cproc -t b32p3 < /tmp/c.i > /tmp/c.qbe
qbe < /tmp/c.qbe > /lib/asm-cache/dma.asm

echo "libc-build: done (13/13 compiled to /lib/asm-cache/)"
