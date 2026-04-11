#include "all.h"

/*
 * B32P3 calling convention:
 *
 *   Arguments:  r4, r5, r6, r7 (first 4 words), then stack
 *   Return:     r1
 *   Caller-save: r1, r2, r3, r4-r7, r12
 *   Callee-save: r8, r9, r10, r11
 *   Global:     r13=SP, r14=FP, r15=RA
 *
 * All types are 32-bit (Kw). No 64-bit integer or FP support.
 * Structs > 8 bytes are passed by pointer.
 * Structs <= 8 bytes are passed in 1 or 2 registers.
 *
 * RCall encoding (in Ref val):
 *   bits 0-3:  gp regs returned (0..1)
 *   bits 4-7:  gp regs passed   (0..4)
 */

typedef struct Class Class;
typedef struct Insl Insl;
typedef struct Params Params;

enum {
	Cptr  = 1, /* replaced by a pointer */
	Cstk  = 2, /* passed on the stack */
	Cval  = 4, /* small struct: pass as word value */
};

struct Class {
	char class;
	Typ *type;
	int reg;
	int cls;
	int nreg;
};

struct Insl {
	Ins i;
	Insl *link;
};

struct Params {
	int ngp;
	int stk;
};

static int gpreg[] = {R4, R5, R6, R7};

bits
b32p3_retregs(Ref r, int p[2])
{
	bits b;
	int ngp;

	assert(rtype(r) == RCall);
	ngp = r.val & 3;
	if (p) {
		p[0] = ngp;
		p[1] = 0;
	}
	b = 0;
	while (ngp--)
		b |= BIT(R1+ngp);
	return b;
}

bits
b32p3_argregs(Ref r, int p[2])
{
	bits b;
	int ngp;

	assert(rtype(r) == RCall);
	ngp = (r.val >> 4) & 15;
	if (p) {
		p[0] = ngp;
		p[1] = 0;
	}
	b = 0;
	while (ngp--)
		b |= BIT(R4+ngp);
	return b;
}

static void
selret(Blk *b, Fn *fn)
{
	int j, k, cty;
	Ref r;

	j = b->jmp.type;

	if (!isret(j) || j == Jret0)
		return;

	r = b->jmp.arg;
	b->jmp.type = Jret0;

	if (j == Jretc) {
		if (typ[fn->retty].size <= 4) {
			/* small struct: load value, return in R1 */
			Ref tmp = newtmp("abi", Kw, fn);
			emit(Ocopy, Kw, TMP(R1), tmp, R);
			emit(Oload, Kw, tmp, r, R);
			cty = 1;
		} else {
			/* large struct: blit to hidden pointer */
			assert(rtype(fn->retr) == RTmp);
			emit(Oblit1, 0, R, INT(typ[fn->retty].size), R);
			emit(Oblit0, 0, R, r, fn->retr);
			cty = 0;
		}
	} else {
		k = j - Jretw;
		if (k == Kl)
			k = Kw; /* 32-bit target: l → w */
		emit(Ocopy, k, TMP(R1), r, R);
		cty = 1;
	}
	b->jmp.arg = CALL(cty);
}

static int
argsclass(Ins *i0, Ins *i1, Class *carg, int retptr)
{
	int ngp, *gp;
	Class *c;
	Ins *i;

	gp = gpreg;
	ngp = 4;
	if (retptr) {
		gp++;
		ngp--;
	}
	for (i=i0, c=carg; i<i1; i++, c++) {
		c->class = 0;
		c->nreg = 0;
		switch (i->op) {
		case Opar:
		case Oarg:
			c->cls = i->cls;
			if (c->cls == Kl)
				c->cls = Kw;
			if (ngp > 0) {
				ngp--;
				c->reg = *gp++;
				c->nreg = 1;
			} else {
				c->class |= Cstk;
			}
			break;
		case Oparc:
		case Oargc:
			c->type = &typ[i->arg[0].val];
			if (c->type->size <= 4) {
				/* small struct (≤4 bytes): pass as word value */
				c->class |= Cval;
			} else {
				/* large struct: pass by pointer */
				c->class |= Cptr;
			}
			c->cls = Kw;
			if (ngp > 0) {
				ngp--;
				c->reg = *gp++;
				c->nreg = 1;
			} else {
				c->class |= Cstk;
			}
			break;
		case Oargv:
			break;
		case Opare:
		case Oarge:
			/* env pointer — use r12 */
			c->reg = R12;
			c->cls = Kw;
			c->nreg = 1;
			break;
		}
	}
	return (gp - gpreg) << 4;
}

static void
stkblob(Ref r, Typ *t, Fn *fn, Insl **ilp)
{
	Insl *il;
	int al;
	uint64_t sz;

	il = alloc(sizeof *il);
	al = t->align - 2;
	if (al < 0)
		al = 0;
	sz = (t->size + 3) & ~3; /* 4-byte aligned */
	il->i = (Ins){Oalloc+al, Kw, r, {getcon(sz, fn)}};
	il->link = *ilp;
	*ilp = il;
}

static void
selcall(Fn *fn, Ins *i0, Ins *i1, Insl **ilp)
{
	Ins *i;
	Class *ca, *c, cr;
	int cty, vararg;
	uint64_t stk, off;
	Ref r, r1;

	ca = alloc((i1-i0) * sizeof ca[0]);
	cr.class = 0;

	if (!req(i1->arg[1], R)) {
		/* aggregate return */
		if (typ[i1->arg[1].val].size <= 4)
			cr.class |= Cval;
		else
			cr.class |= Cptr;
	}

	cty = argsclass(i0, i1, ca, cr.class & Cptr);

	/* detect vararg call (Oargv present in args) */
	vararg = 0;
	for (i=i0; i<i1; i++)
		if (i->op == Oargv) {
			vararg = 1;
			break;
		}

	stk = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv)
			continue;
		if (c->class & Cptr) {
			i->arg[0] = newtmp("abi", Kw, fn);
			stkblob(i->arg[0], c->type, fn, ilp);
			i->op = Oarg;
		}
		if (c->class & Cstk)
			stk += 4;
	}
	stk = (stk + 3) & ~3; /* align to 4 */
	if (vararg) {
		/* reserve 8 (FP+RA) + 16 (r4-r7 save area) = 24 bytes */
		stk += 24;
	} else if (stk) {
		stk += 8; /* reserve space for callee's FP+RA save area */
	}
	if (stk)
		emit(Osalloc, Kl, R, getcon(-stk, fn), R);

	if (cr.class & Cptr) {
		/* large aggregate return: allocate space, pass pointer in R4 */
		stkblob(i1->to, &typ[i1->arg[1].val], fn, ilp);
		emit(Ocopy, Kw, R, TMP(R1), R);
		cty |= 0;
	} else if (cr.class & Cval) {
		/* small aggregate return: value in R1, store to alloc'd space.
		 * Must use a regcpy (Ocopy from R1) so the spiller's dopm
		 * detects the preceding call and handles register effects. */
		Ref tmp;
		stkblob(i1->to, &typ[i1->arg[1].val], fn, ilp);
		tmp = newtmp("abi", Kw, fn);
		emit(Ostorew, Kw, R, tmp, i1->to);
		emit(Ocopy, Kw, tmp, TMP(R1), R);
		cty |= 1;
	} else {
		emit(Ocopy, i1->cls == Kl ? Kw : i1->cls,
			i1->to, TMP(R1), R);
		cty |= 1;
	}

	emit(Ocall, 0, R, i1->arg[0], CALL(cty));

	if (cr.class & Cptr)
		emit(Ocopy, Kw, TMP(R4), i1->to, R);

	/* move arguments into registers */
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv || c->class & Cstk)
			continue;
		if (i->op == Oargc) {
			if (c->class & Cval) {
				/* small struct: load word value from source */
				r = newtmp("abi", Kw, fn);
				emit(Ocopy, Kw, TMP(c->reg), r, R);
				emit(Oload, Kw, r, i->arg[1], R);
			} else {
				/* large struct: blit to allocated space */
				emit(Oblit1, 0, R, INT(c->type->size), R);
				emit(Oblit0, 0, R, i->arg[1], i->arg[0]);
				emit(Ocopy, Kw, TMP(c->reg), i->arg[0], R);
			}
		} else {
			emit(Ocopy, c->cls, TMP(c->reg), i->arg[0], R);
		}
	}

	if (!stk)
		return;

	/* populate the stack */
	off = vararg ? 24 : 8; /* skip callee's FP+RA (+ vararg) save area */
	r = newtmp("abi", Kw, fn);
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oargv || !(c->class & Cstk))
			continue;
		r1 = newtmp("abi", Kw, fn);
		if (c->class & Cval) {
			/* small struct: load value, store to stack */
			Ref val = newtmp("abi", Kw, fn);
			emit(Ostorew, Kw, R, val, r1);
			emit(Oadd, Kw, r1, r, getcon(off, fn));
			emit(Oload, Kw, val, i->arg[1], R);
		} else {
			emit(Ostorew, Kw, R, i->arg[0], r1);
			emit(Oadd, Kw, r1, r, getcon(off, fn));
		}
		off += 4;
	}
	if (off == (vararg ? 24 : 8)) {
		/* no stack args were stored; use R as dest to
		 * prevent the alloc from being optimized away */
		emit(Osalloc, Kl, R, getcon(stk, fn), R);
	} else {
		emit(Osalloc, Kl, r, getcon(stk, fn), R);
	}
}

static void
selpar(Fn *fn, Ins *i0, Ins *i1, Params *outp)
{
	Class *ca, *c, cr;
	Insl *il;
	Ins *i;
	int s, cty;
	Ref r;

	ca = alloc((i1-i0) * sizeof ca[0]);
	cr.class = 0;
	curi = &insb[NIns];

	if (fn->retty >= 0) {
		if (typ[fn->retty].size > 4) {
			cr.class |= Cptr;
			fn->retr = newtmp("abi", Kw, fn);
			emit(Ocopy, Kw, fn->retr, TMP(R4), R);
		}
		/* small struct return: value in R1, no hidden pointer */
	}

	cty = argsclass(i0, i1, ca, cr.class & Cptr);
	fn->reg = b32p3_argregs(CALL(cty), 0);

	il = 0;
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oparc && !(c->class & Cptr) && c->nreg > 0) {
			stkblob(i->to, c->type, fn, &il);
		}
	}

	/* incoming frame layout:
	 * slot -1, -2: saved fp, saved ra (pushed by callee prologue)
	 * if vararg:
	 *   slot -3..-6: saved r4-r7 (arg regs, saved by callee prologue)
	 * then: stack arguments from caller
	 */
	s = 2 + 4 * fn->vararg; /* skip saved fp/ra and vararg save area */
	for (i=i0, c=ca; i<i1; i++, c++) {
		if (i->op == Oparc && !(c->class & Cptr)) {
			if (c->nreg == 0) {
				fn->tmp[i->to.val].slot = -s;
				s++;
				continue;
			}
			/* store reg value to stack blob via temp
			 * (physical regs can't be storew args directly) */
			Ref val = newtmp("abi", Kw, fn);
			r = newtmp("abi", Kw, fn);
			emit(Ostorew, Kw, R, val, r);
			emit(Ocopy, Kw, r, i->to, R);
			emit(Ocopy, Kw, val, TMP(c->reg), R);
		} else if (c->class & Cstk) {
			emit(Oload, c->cls == Kl ? Kw : c->cls,
				i->to, SLOT(-s), R);
			s++;
		} else if (c->nreg > 0) {
			emit(Ocopy, c->cls == Kl ? Kw : c->cls,
				i->to, TMP(c->reg), R);
		}
	}

	/* emit allocs AFTER param copies (emit builds backward,
	 * so allocs will appear first in execution order) */
	for (; il; il=il->link)
		emiti(il->i);

	outp->stk = s;
	outp->ngp = (cty >> 4) & 15;
}

static void
selvaarg(Fn *fn, Ins *i)
{
	Ref loc, newloc;

	loc = newtmp("abi", Kw, fn);
	newloc = newtmp("abi", Kw, fn);
	emit(Ostorew, Kw, R, newloc, i->arg[0]);
	emit(Oadd, Kw, newloc, loc, getcon(4, fn));
	emit(Oload, i->cls == Kl ? Kw : i->cls, i->to, loc, R);
	emit(Oload, Kw, loc, i->arg[0], R);
}

static void
selvastart(Fn *fn, Params p, Ref ap)
{
	Ref rsave;
	int s;

	rsave = newtmp("abi", Kw, fn);
	emit(Ostorew, Kw, R, rsave, ap);
	s = p.stk > 2 + 4 * fn->vararg ? p.stk : 2 + p.ngp;
	emit(Oaddr, Kw, rsave, SLOT(-s), R);
}

static void
lowerblk(Fn *fn, Blk *b, Insl **ilp, Params *p)
{
	Ins *i, *i0;

	curi = &insb[NIns];
	selret(b, fn);
	for (i=&b->ins[b->nins]; i!=b->ins;)
		switch ((--i)->op) {
		default:
			emiti(*i);
			break;
		case Ocall:
			for (i0=i; i0>b->ins; i0--)
				if (!isarg((i0-1)->op))
					break;
			selcall(fn, i0, i, ilp);
			i = i0;
			break;
		case Ovastart:
			selvastart(fn, *p, i->arg[0]);
			break;
		case Ovaarg:
			selvaarg(fn, i);
			break;
		case Oarg:
		case Oargc:
			die("unreachable");
		}
	if (b == fn->start)
		for (; *ilp; *ilp=(*ilp)->link)
			emiti((*ilp)->i);
	b->nins = &insb[NIns] - curi;
	idup(&b->ins, curi, b->nins);
}

static void
lowerparams(Fn *fn, Params *outp)
{
	Blk *b;
	Ins *i, *i0, *ip;
	int n;

	b = fn->start;
	for (i=b->ins; i<&b->ins[b->nins]; i++)
		if (!ispar(i->op))
			break;
	selpar(fn, b->ins, i, outp);
	n = b->nins - (i - b->ins) + (&insb[NIns] - curi);
	i0 = alloc(n * sizeof(Ins));
	ip = icpy(ip = i0, curi, &insb[NIns] - curi);
	ip = icpy(ip, i, &b->ins[b->nins] - i);
	b->nins = n;
	b->ins = i0;
}

void
b32p3_abi(Fn *fn)
{
	Blk *b;
	Insl *il;
	Params p;

	for (b=fn->start; b; b=b->link)
		b->visit = 0;

	lowerparams(fn, &p);

	/* lower calls, returns, and vararg instructions */
	il = 0;
	b = fn->start;
	do {
		if (!(b = b->link))
			b = fn->start;
		if (b->visit)
			continue;
		lowerblk(fn, b, &il, &p);
	} while (b != fn->start);

	if (debug['A']) {
		fprintf(stderr, "\n> After ABI lowering:\n");
		printfn(fn, stderr);
	}
}
