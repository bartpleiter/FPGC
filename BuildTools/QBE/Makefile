.POSIX:
.SUFFIXES: .o .c

COMMOBJ  = main.o util.o parse.o abi.o cfg.o mem.o ssa.o alias.o load.o \
           copy.o fold.o simpl.o live.o spill.o rega.o emit.o
B32P3OBJ = b32p3/targ.o b32p3/abi.o b32p3/isel.o b32p3/emit.o
OBJ      = $(COMMOBJ) $(B32P3OBJ)

SRCALL   = $(OBJ:.o=.c)
OUTDIR   = output

CC       = cc
CFLAGS   = -std=c99 -O2 -Wall -Wextra -Wpedantic

$(OUTDIR)/qbe: $(OBJ)
	@mkdir -p $(OUTDIR)
	$(CC) $(LDFLAGS) $(OBJ) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): all.h ops.h
$(B32P3OBJ): b32p3/all.h

clean:
	rm -f *.o b32p3/*.o output/qbe
