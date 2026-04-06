#include "../all.h"

typedef struct B32p3Op B32p3Op;

enum B32p3Reg {
	/* B32P3 has 16 GPRs:
	 *   r0  = hardwired zero (never allocated)
	 *   r1  = return value (caller-save)
	 *   r2  = temporary (caller-save)
	 *   r3  = temporary (caller-save)
	 *   r4  = arg0 (caller-save)
	 *   r5  = arg1 (caller-save)
	 *   r6  = arg2 (caller-save)
	 *   r7  = arg3 (caller-save)
	 *   r8  = callee-save
	 *   r9  = callee-save
	 *   r10 = callee-save
	 *   r11 = callee-save
	 *   r12 = temporary (caller-save)
	 *   r13 = SP (stack pointer, global)
	 *   r14 = FP (frame pointer, global)
	 *   r15 = RA (return address, global)
	 *
	 * Register ordering follows QBE convention:
	 * caller-save first, then callee-save, then global.
	 */

	/* caller-save temporaries */
	R1 = RXX + 1, R2, R3,
	R4, R5, R6, R7,  /* argument registers */

	/* callee-save */
	R8, R9, R10, R11,

	/* reserved for emit.c scratch (not allocatable) */
	R12,

	/* globally live (never allocated) */
	R13, /* SP */
	R14, /* FP */
	R15, /* RA */

	/* r0 is not enumerated — it's hardwired zero,
	 * handled specially in emit (like rv64's x0) */

	NGPR = R11 - R1 + 1,  /* 11 allocatable GPRs (R1-R11; R12 reserved for emit scratch) */
	NFPR = 0,              /* no FP register class */
	NGPS = R7 - R1 + 1,   /* 7 caller-save */
	NFPS = 0,
	NCLR = R11 - R8 + 1,  /* 4 callee-save */
};
MAKESURE(b32p3_reg_not_tmp, R15 < (int)Tmp0);

struct B32p3Op {
	char imm;
};

/* targ.c */
extern int b32p3_rsave[];
extern int b32p3_rclob[];
extern B32p3Op b32p3_op[];

/* abi.c */
bits b32p3_retregs(Ref, int[2]);
bits b32p3_argregs(Ref, int[2]);
void b32p3_abi(Fn *);

/* isel.c */
void b32p3_isel(Fn *);

/* emit.c */
void b32p3_emitfn(Fn *, FILE *);
void b32p3_emitfin(FILE *);
