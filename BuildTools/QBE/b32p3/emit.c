#include "all.h"

/*
 * B32P3 assembly emission for ASMPY assembler.
 *
 * ASMPY syntax:
 *   instruction arg1 arg2 arg3    (space-separated)
 *   Label:                         (on its own line)
 *   .dw value [value ...]          (data words)
 *   ; comment
 *
 * Key instructions:
 *   add  rA rB rD   ; rD = rA + rB  (or: add rA imm rD)
 *   sub  rA rB rD   ; rD = rA - rB
 *   read off rA rD  ; rD = mem[rA + off]     (word load)
 *   write off rA rS ; mem[rA + off] = rS     (word store)
 *   readb off rA rD ; rD = byte mem[rA + off] (byte load, zero-extend)
 *   writeb off rA rS; byte mem[rA + off] = rS (byte store)
 *   load imm rD     ; rD = sign-extend-16(imm)
 *   loadhi imm rD   ; rD[31:16] = imm
 *   jump addr       ; PC = addr  (direct jump, no HW stack push)
 *   jumpr off rA    ; PC = rA + off (register indirect)
 *   beq rA rB off   ; if rA == rB: PC += off
 *   bne rA rB off   ; if rA != rB: PC += off
 *   bgts rA rB off  ; if (signed)rA > rB: PC += off
 *   bges rA rB off  ; if (signed)rA >= rB: PC += off
 *   blts rA rB off  ; if (signed)rA < rB: PC += off
 *   bles rA rB off  ; if (signed)rA <= rB: PC += off
 *   bgt rA rB off   ; if (unsigned)rA > rB: PC += off
 *   bge rA rB off   ; if (unsigned)rA >= rB: PC += off
 *   blt rA rB off   ; if (unsigned)rA < rB: PC += off
 *   ble rA rB off   ; if (unsigned)rA <= rB: PC += off
 *   slt rA rB rD    ; rD = (signed)rA < rB ? 1 : 0
 *   sltu rA rB rD   ; rD = (unsigned)rA < rB ? 1 : 0
 *   savpc rD        ; rD = PC (of this instruction)
 *   or  rA rB rD    ; rD = rA | rB (also used for move: or r0 rS rD)
 *   and rA rB rD    ; rD = rA & rB
 *   xor rA rB rD    ; rD = rA ^ rB
 *   shiftl rA rB rD ; rD = rA << rB
 *   shiftr rA rB rD ; rD = rA >> rB  (logical)
 *   shiftrs rA rB rD; rD = rA >>> rB (arithmetic)
 *   mults rA rB rD  ; rD = (signed)rA * rB
 *   multu rA rB rD  ; rD = (unsigned)rA * rB
 *   divs rA rB rD   ; rD = (signed)rA / rB
 *   divu rA rB rD   ; rD = (unsigned)rA / rB
 *   mods rA rB rD   ; rD = (signed)rA % rB
 *   modu rA rB rD   ; rD = (unsigned)rA % rB
 *   not rA rD       ; rD = ~rA
 *   nop             ; no operation
 *   halt            ; halt CPU
 *
 * Calling convention:
 *   savpc r15       ; r15 = return address
 *   add r15 12 r15  ; adjust to point past the jump instruction
 *   jump Label      ; jump to callee
 *   ; ...return here...
 *
 * Return:
 *   jumpr 0 r15     ; return to caller
 */

enum {
	Ki = -1, /* matches Kw and Kl */
	Ka = -2, /* matches all classes */
};

static struct {
	short op;
	short cls;
	char *asm_str;
} omap[] = {
	{ Oadd,    Ki, "add %0 %1 %=" },
	{ Osub,    Ki, "sub %0 %1 %=" },
	{ Oneg,    Ki, "sub r0 %0 %=" },
	{ Omul,    Ki, "mults %0 %1 %=" },
	{ Odiv,    Ki, "divs %0 %1 %=" },
	{ Orem,    Ki, "mods %0 %1 %=" },
	{ Oudiv,   Ki, "divu %0 %1 %=" },
	{ Ourem,   Ki, "modu %0 %1 %=" },
	{ Oand,    Ki, "and %0 %1 %=" },
	{ Oor,     Ki, "or %0 %1 %=" },
	{ Oxor,    Ki, "xor %0 %1 %=" },
	{ Osar,    Ki, "shiftrs %0 %1 %=" },
	{ Oshr,    Ki, "shiftr %0 %1 %=" },
	{ Oshl,    Ki, "shiftl %0 %1 %=" },
	{ Ocsltl,  Ki, "slt %0 %1 %=" },
	{ Ocultl,  Ki, "sltu %0 %1 %=" },
	{ Ostoreb, Kw, "writeb %M1 %0" },
	{ Ostoreh, Kw, "writeh %M1 %0" },
	{ Ostorew, Kw, "write %M1 %0" },
	{ Ostorel, Ki, "write %M1 %0" },  /* 32-bit target: storel = storew */
	{ Oloadsb, Ki, "readbs %M0 %=" },
	{ Oloadub, Ki, "readb %M0 %=" },
	{ Oloadsh, Ki, "readhs %M0 %=" },
	{ Oloaduh, Ki, "readh %M0 %=" },
	{ Oloadsw, Ki, "read %M0 %=" },
	{ Oloaduw, Ki, "read %M0 %=" },
	{ Oload,   Kw, "read %M0 %=" },
	{ Oload,   Kl, "read %M0 %=" },   /* pointers are 32-bit */
	{ Oextsb,  Ki, "readbs 0 %0 %=" }, /* sign-extend byte via readbs */
	{ Oextub,  Ki, "and %0 0xFF %=" },
	{ Oextsh,  Ki, "readhs 0 %0 %=" },
	{ Oextuh,  Ki, "and %0 0xFFFF %=" },
	{ Oextsw,  Ki, "or r0 %0 %=" },   /* no-op on 32-bit */
	{ Oextuw,  Ki, "or r0 %0 %=" },   /* no-op on 32-bit */
	{ Oreqz,   Ki, "sltu %0 1 %=" },  /* result = (arg == 0) */
	{ Ornez,   Ki, "sltu r0 %0 %=" }, /* result = (arg != 0) */
	{ Ocopy,   Ki, "or r0 %0 %=" },
	{ Oswap,   Ki, "or r0 %0 r12\n  or r0 %1 %0\n  or r0 r12 %1" },
	{ Ocall,   Kw, "jump %0" },
	{ NOp, 0, 0 }
};

static char *rname[] = {
	[R1]  = "r1",
	[R2]  = "r2",
	[R3]  = "r3",
	[R4]  = "r4",
	[R5]  = "r5",
	[R6]  = "r6",
	[R7]  = "r7",
	[R8]  = "r8",
	[R9]  = "r9",
	[R10] = "r10",
	[R11] = "r11",
	[R12] = "r12",
	[R13] = "r13",
	[R14] = "r14",
	[R15] = "r15",
};

static int64_t
slot(Ref r, Fn *fn)
{
	int s;

	s = rsval(r);
	assert(s <= fn->slot);
	if (s < 0)
		return 4 * -s;   /* incoming args: positive offset from FP */
	else
		return -4 * (fn->slot - s); /* locals: negative offset from FP */
}

static void
emitaddr(Con *c, FILE *f)
{
	assert(c->sym.type == SGlo);
	fputs(str(c->sym.id), f);
	if (c->bits.i)
		fprintf(f, "+%"PRIi64, c->bits.i);
}

static void
emitf(char *s, Ins *i, Fn *fn, FILE *f)
{
	Ref r;
	int c;
	Con *pc;
	int64_t offset;

	fputs("  ", f);
	for (;;) {
		while ((c = *s++) != '%')
			if (!c) {
				fputc('\n', f);
				return;
			} else
				fputc(c, f);
		switch ((c = *s++)) {
		default:
			die("invalid escape");
		case '=':
		case '0':
			r = c == '=' ? i->to : i->arg[0];
			assert(isreg(r));
			fputs(rname[r.val], f);
			break;
		case '1':
			r = i->arg[1];
			switch (rtype(r)) {
			default:
				die("invalid second argument");
			case RTmp:
				assert(isreg(r));
				fputs(rname[r.val], f);
				break;
			case RCon:
				pc = &fn->con[r.val];
				assert(pc->type == CBits);
				fprintf(f, "%d", (int)pc->bits.i);
				break;
			}
			break;
		case 'M':
			c = *s++;
			assert(c == '0' || c == '1');
			r = i->arg[c - '0'];
			switch (rtype(r)) {
			default:
				die("invalid address argument");
			case RTmp:
				fprintf(f, "0 %s", rname[r.val]);
				break;
			case RCon:
				/* Global addresses should be pre-loaded
				 * into r12 by emitins() before we get here */
				pc = &fn->con[r.val];
				assert(pc->type == CAddr);
				fprintf(f, "0 r12");
				break;
			case RSlot:
				offset = slot(r, fn);
				fprintf(f, "%d r14", (int)offset);
				break;
			}
			break;
		}
	}
}

static void
loadcon(Con *c, int r, FILE *f)
{
	char *rn;
	int64_t n;

	rn = rname[r];
	switch (c->type) {
	case CAddr:
		fprintf(f, "  addr2reg ");
		emitaddr(c, f);
		fprintf(f, " %s\n", rn);
		break;
	case CBits:
		n = c->bits.i;
		n = (int32_t)n; /* truncate to 32-bit */
		if (n >= -32768 && n < 32768) {
			fprintf(f, "  load %d %s\n", (int)n, rn);
		} else {
			fprintf(f, "  load32 %d %s\n", (int)n, rn);
		}
		break;
	default:
		die("invalid constant");
	}
}

static void
fixmem(Ref *pr, Fn *fn, FILE *f)
{
	Ref r;
	int64_t s;

	r = *pr;
	if (rtype(r) == RSlot) {
		s = slot(r, fn);
		if (s < -32768 || s > 32767) {
			fprintf(f, "  load32 %d r12\n", (int)s);
			fprintf(f, "  add r14 r12 r12\n");
			*pr = TMP(R12);
		}
	}
}

static void
fixaddr(Ref *pr, Fn *fn, FILE *f)
{
	/* If a memory operand is a global address (CAddr constant),
	 * emit addr2reg to load the address into r12 first,
	 * then replace the operand with r12. */
	Ref r;
	Con *c;

	r = *pr;
	if (rtype(r) == RCon) {
		c = &fn->con[r.val];
		if (c->type == CAddr) {
			fprintf(f, "  addr2reg ");
			emitaddr(c, f);
			fprintf(f, " r12\n");
			*pr = TMP(R12);
		}
	}
}

static void
emitins(Ins *i, Fn *fn, FILE *f)
{
	int o;
	char *rn;
	int64_t s;
	Con *con;

	switch (i->op) {
	default:
		if (isload(i->op)) {
			fixaddr(&i->arg[0], fn, f);
			fixmem(&i->arg[0], fn, f);
		} else if (isstore(i->op)) {
			fixaddr(&i->arg[1], fn, f);
			fixmem(&i->arg[1], fn, f);
		}
	Table:
		for (o=0;; o++) {
			if (omap[o].op == NOp)
				die("no match for %s(%c)",
					optab[i->op].name, "wlsd"[i->cls]);
			if (omap[o].op == i->op)
			if (omap[o].cls == i->cls || omap[o].cls == Ka
			|| (omap[o].cls == Ki && KBASE(i->cls) == 0))
				break;
		}
		emitf(omap[o].asm_str, i, fn, f);
		break;
	case Ocopy:
		if (req(i->to, i->arg[0]))
			break;
		if (rtype(i->to) == RSlot) {
			switch (rtype(i->arg[0])) {
			case RSlot:
			case RCon:
				die("unimplemented copy slot/con to slot");
				break;
			default:
				assert(isreg(i->arg[0]));
				i->arg[1] = i->to;
				i->to = R;
				i->op = Ostorew;
				fixmem(&i->arg[1], fn, f);
				goto Table;
			}
			break;
		}
		assert(isreg(i->to));
		switch (rtype(i->arg[0])) {
		case RCon:
			loadcon(&fn->con[i->arg[0].val], i->to.val, f);
			break;
		case RSlot:
			i->op = Oload;
			fixmem(&i->arg[0], fn, f);
			goto Table;
		default:
			assert(isreg(i->arg[0]));
			goto Table;
		}
		break;
	case Onop:
		break;
	case Oaddr:
		assert(rtype(i->arg[0]) == RSlot);
		rn = rname[i->to.val];
		s = slot(i->arg[0], fn);
		if (s >= -32768 && s < 32768) {
			fprintf(f, "  add r14 %d %s\n", (int)s, rn);
		} else {
			fprintf(f,
				"  load32 %d %s\n"
				"  add r14 %s %s\n",
				(int)s, rn, rn, rn
			);
		}
		break;
	case Ocall:
		switch (rtype(i->arg[0])) {
		case RCon:
			con = &fn->con[i->arg[0].val];
			if (con->type != CAddr
			|| con->sym.type != SGlo
			|| con->bits.i)
				goto Invalid;
			fprintf(f,
				"  savpc r15\n"
				"  add r15 12 r15\n"
				"  jump %s\n",
				str(con->sym.id)
			);
			break;
		case RTmp:
			assert(isreg(i->arg[0]));
			fprintf(f,
				"  savpc r15\n"
				"  add r15 12 r15\n"
				"  jumpr 0 %s\n",
				rname[i->arg[0].val]
			);
			break;
		default:
		Invalid:
			die("invalid call argument");
		}
		break;
	case Osalloc:
		/* sub sp, sp, size */
		assert(isreg(i->arg[0]));
		fprintf(f, "  sub r13 %s r13\n", rname[i->arg[0].val]);
		if (!req(i->to, R))
			fprintf(f, "  or r0 r13 %s\n", rname[i->to.val]);
		break;
	case Odbgloc:
		emitdbgloc(i->arg[0].val, i->arg[1].val, f);
		break;
	}
}

/*
 * Stack frame layout:
 *
 *   +=============+
 *   |  caller's   |
 *   |  stack args |
 *   +-------------+ <- old SP
 *   |  saved FP   |  [FP+0]
 *   |  saved RA   |  [FP+4]
 *   +-------------+ <- FP
 *   |    ...      |
 *   | spill slots |  [FP-4], [FP-8], ...
 *   |   locals    |
 *   |    ...      |
 *   +-------------+
 *   | callee-save |
 *   |  registers  |
 *   +=============+ <- SP
 */

void
b32p3_emitfn(Fn *fn, FILE *f)
{
	static int id0;
	int lbl, neg, off, frame, *pr;
	Blk *b, *s;
	Ins *i;

	/* emit function label */
	fprintf(f, "\n.text\n");
	fprintf(f, "; function %s\n", fn->name);
	if (fn->lnk.export)
		fprintf(f, ".global %s\n", fn->name);
	fprintf(f, "%s:\n", fn->name);

	/* prologue: save FP and RA, set up new frame */
	fprintf(f, "  write 0 r13 r14\n");   /* save old FP at [SP] */
	fprintf(f, "  write 4 r13 r15\n");   /* save RA at [SP+4] */
	fprintf(f, "  or r0 r13 r14\n");     /* FP = SP */

	/* calculate frame size: spill slots + callee-save regs */
	frame = 4 * fn->slot;  /* spill slots */
	for (pr=b32p3_rclob; *pr>=0; pr++) {
		if (fn->reg & BIT(*pr))
			frame += 4;
	}
	frame = (frame + 3) & ~3; /* align to 4 */
	frame += 8; /* space for saved FP and RA */

	if (frame > 0)
		fprintf(f, "  sub r13 %d r13\n", frame);

	/* save callee-save registers */
	off = 8; /* after FP and RA save area */
	for (pr=b32p3_rclob; *pr>=0; pr++) {
		if (fn->reg & BIT(*pr)) {
			fprintf(f, "  write -%d r14 %s\n",
				off + 4 * fn->slot, rname[*pr]);
			off += 4;
		}
	}

	/* emit blocks */
	for (lbl=0, b=fn->start; b; b=b->link) {
		if (lbl || b->npred > 1)
			fprintf(f, ".L%d:\n", id0+b->id);
		for (i=b->ins; i!=&b->ins[b->nins]; i++)
			emitins(i, fn, f);
		lbl = 1;
		switch (b->jmp.type) {
		case Jhlt:
			fprintf(f, "  halt\n");
			break;
		case Jret0:
			/* epilogue: restore callee-saves, FP, RA, return */
			off = 8;
			for (pr=b32p3_rclob; *pr>=0; pr++) {
				if (fn->reg & BIT(*pr)) {
					fprintf(f, "  read -%d r14 %s\n",
						off + 4 * fn->slot, rname[*pr]);
					off += 4;
				}
			}
			fprintf(f,
				"  or r0 r14 r13\n"   /* SP = FP */
				"  read 4 r14 r15\n"  /* restore RA */
				"  read 0 r14 r14\n"  /* restore FP */
				"  jumpr 0 r15\n"     /* return */
			);
			break;
		case Jjmp:
		Jmp:
			if (b->s1 != b->link)
				fprintf(f, "  jump .L%d\n", id0+b->s1->id);
			else
				lbl = 0;
			break;
		case Jjnz:
			neg = 0;
			if (b->link == b->s2) {
				s = b->s1;
				b->s1 = b->s2;
				b->s2 = s;
				neg = 1;
			}
			assert(isreg(b->jmp.arg));
			fprintf(f,
				"  %s %s r0 8\n"
				"  jump .L%d\n",
				neg ? "beq" : "bne",
				rname[b->jmp.arg.val],
				id0+b->s2->id
			);
			goto Jmp;
		}
	}
	id0 += fn->nblk;
	fprintf(f, "\n");
}

void
b32p3_emitfin(FILE *f)
{
	/* no ELF finalization — just flat assembly for ASMPY */
	(void)f;
}
