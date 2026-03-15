#include "all.h"

/*
 * B32P3 instruction selection.
 *
 * B32P3 has 16-bit signed immediates for most instructions.
 * No floating-point support in this backend.
 * All pointers and integers are 32-bit (Kw).
 */

static int
memarg(Ref *r, int op, Ins *i)
{
	if (isload(op) || op == Ocall)
		return r == &i->arg[0];
	if (isstore(op))
		return r == &i->arg[1];
	return 0;
}

static int
immarg(Ref *r, int op, Ins *i)
{
	return b32p3_op[op].imm && r == &i->arg[1];
}

static void
fixarg(Ref *r, int k, Ins *i, Fn *fn)
{
	Ref r0, r1;
	int s, op;
	Con *c;

	r0 = r1 = *r;
	op = i ? i->op : Ocopy;

	/* B32P3 is 32-bit only: treat Kl as Kw */
	if (k == Kl)
		k = Kw;

	switch (rtype(r0)) {
	case RCon:
		c = &fn->con[r0.val];
		if (c->type == CAddr && memarg(r, op, i))
			break;
		if (c->type == CBits && immarg(r, op, i))
		if (-32768 <= c->bits.i && c->bits.i < 32768)
			break;
		r1 = newtmp("isel", k, fn);
		emit(Ocopy, k, r1, r0, R);
		break;
	case RTmp:
		if (isreg(r0))
			break;
		s = fn->tmp[r0.val].slot;
		if (s != -1) {
			if (memarg(r, op, i)) {
				r1 = SLOT(s);
				break;
			}
			r1 = newtmp("isel", k, fn);
			emit(Oaddr, k, r1, SLOT(s), R);
			break;
		}
		/* On a 32-bit target, Kl temps should not appear.
		 * If they do (e.g. from pointer ops), treat as Kw.
		 */
		if (k == Kw && fn->tmp[r0.val].cls == Kl) {
			/* no extension needed on 32-bit */
			break;
		}
		break;
	}
	*r = r1;
}

static void
negate(Ref *pr, Fn *fn)
{
	Ref r;

	r = newtmp("isel", Kw, fn);
	emit(Oxor, Kw, *pr, r, getcon(1, fn));
	*pr = r;
}

static void
selcmp(Ins i, int k, int op, Fn *fn)
{
	Ins *icmp;
	Ref r;
	int swap, neg;

	/* B32P3 is 32-bit only */
	if (k == Kl)
		k = Kw;

	switch (op) {
	case Cieq:
		r = newtmp("isel", k, fn);
		emit(Oreqz, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = curi;
		fixarg(&icmp->arg[0], k, icmp, fn);
		fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case Cine:
		r = newtmp("isel", k, fn);
		emit(Ornez, i.cls, i.to, r, R);
		emit(Oxor, k, r, i.arg[0], i.arg[1]);
		icmp = curi;
		fixarg(&icmp->arg[0], k, icmp, fn);
		fixarg(&icmp->arg[1], k, icmp, fn);
		return;
	case Cisge: swap = 0; neg = 1; break;
	case Cisgt: swap = 1; neg = 0; break;
	case Cisle: swap = 1; neg = 1; break;
	case Cislt: swap = 0; neg = 0; break;
	case Ciuge: swap = 0; neg = 1; break;
	case Ciugt: swap = 1; neg = 0; break;
	case Ciule: swap = 1; neg = 1; break;
	case Ciult: swap = 0; neg = 0; break;
	default:
		die("b32p3: unsupported comparison %d", op);
	}

	i.op = (op >= Ciuge) ? Ocultl : Ocsltl;
	if (swap) {
		r = i.arg[0];
		i.arg[0] = i.arg[1];
		i.arg[1] = r;
	}
	if (neg)
		negate(&i.to, fn);
	emiti(i);
	icmp = curi;
	fixarg(&icmp->arg[0], k, icmp, fn);
	fixarg(&icmp->arg[1], k, icmp, fn);
}

static void
sel(Ins i, Fn *fn)
{
	Ins *i0;
	int ck, cc;

	if (INRANGE(i.op, Oalloc, Oalloc1)) {
		i0 = curi - 1;
		salloc(i.to, i.arg[0], fn);
		fixarg(&i0->arg[0], Kw, i0, fn);
		return;
	}
	if (iscmp(i.op, &ck, &cc)) {
		selcmp(i, ck, cc, fn);
		return;
	}
	if (i.op != Onop) {
		emiti(i);
		i0 = curi;
		fixarg(&i0->arg[0], argcls(&i, 0), i0, fn);
		fixarg(&i0->arg[1], argcls(&i, 1), i0, fn);
	}
}

static void
seljmp(Blk *b, Fn *fn)
{
	if (b->jmp.type == Jjnz)
		fixarg(&b->jmp.arg, Kw, 0, fn);
}

void
b32p3_isel(Fn *fn)
{
	Blk *b, **sb;
	Ins *i;
	Phi *p;
	uint n;
	int al;
	int64_t sz;

	/* assign slots to fast allocs */
	b = fn->start;
	for (al=Oalloc, n=4; al<=Oalloc1; al++, n*=2)
		for (i=b->ins; i<&b->ins[b->nins]; i++)
			if (i->op == al) {
				if (rtype(i->arg[0]) != RCon)
					break;
				sz = fn->con[i->arg[0].val].bits.i;
				if (sz < 0 || sz >= INT_MAX-15)
					err("invalid alloc size %"PRId64, sz);
				sz = (sz + n-1) & -n;
				sz /= 4; /* 4 bytes per slot */
				if (sz > INT_MAX - fn->slot)
					die("alloc too large");
				fn->tmp[i->to.val].slot = fn->slot;
				fn->slot += sz;
				*i = (Ins){.op = Onop};
			}

	for (b=fn->start; b; b=b->link) {
		curi = &insb[NIns];
		for (sb=(Blk*[3]){b->s1, b->s2, 0}; *sb; sb++)
			for (p=(*sb)->phi; p; p=p->link) {
				for (n=0; p->blk[n] != b; n++)
					assert(n+1 < p->narg);
				fixarg(&p->arg[n], p->cls, 0, fn);
			}
		seljmp(b, fn);
		for (i=&b->ins[b->nins]; i!=b->ins;)
			sel(*--i, fn);
		b->nins = &insb[NIns] - curi;
		idup(&b->ins, curi, b->nins);
	}

	if (debug['I']) {
		fprintf(stderr, "\n> After instruction selection:\n");
		printfn(fn, stderr);
	}
}
