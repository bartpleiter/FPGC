.POSIX:

BACKEND=qbe

CC=cc
CFLAGS=-std=c99 -O2 -Wall -Wextra -Wpedantic
OUTDIR=output

.PHONY: all
all: $(OUTDIR)/cproc-qbe

SRC=\
	attr.c\
	decl.c\
	eval.c\
	expr.c\
	init.c\
	main.c\
	map.c\
	pp.c\
	scan.c\
	scope.c\
	stmt.c\
	targ.c\
	token.c\
	tree.c\
	type.c\
	utf.c\
	util.c\
	$(BACKEND).c
OBJ=$(SRC:.c=.o)

$(OUTDIR)/cproc-qbe: $(OBJ)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

attr.o    : attr.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ attr.c
decl.o    : decl.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ decl.c
eval.o    : eval.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ eval.c
expr.o    : expr.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ expr.c
init.o    : init.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ init.c
main.o    : main.c    util.h cc.h arg.h ; $(CC) $(CFLAGS) -c -o $@ main.c
map.o     : map.c     util.h            ; $(CC) $(CFLAGS) -c -o $@ map.c
pp.o      : pp.c      util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ pp.c
qbe.o     : qbe.c     util.h cc.h ops.h ; $(CC) $(CFLAGS) -c -o $@ qbe.c
scan.o    : scan.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ scan.c
scope.o   : scope.c   util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ scope.c
stmt.o    : stmt.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ stmt.c
targ.o    : targ.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ targ.c
token.o   : token.c   util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ token.c
tree.o    : tree.c    util.h            ; $(CC) $(CFLAGS) -c -o $@ tree.c
type.o    : type.c    util.h cc.h       ; $(CC) $(CFLAGS) -c -o $@ type.c
utf.o     : utf.c     utf.h             ; $(CC) $(CFLAGS) -c -o $@ utf.c
util.o    : util.c    util.h            ; $(CC) $(CFLAGS) -c -o $@ util.c

.PHONY: clean
clean:
	rm -f *.o $(OUTDIR)/cproc-qbe
