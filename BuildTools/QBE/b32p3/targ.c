#include "all.h"

B32p3Op b32p3_op[NOp] = {
#define O(op, t, x) [O##op] =
#define V(imm) { imm },
#include "../ops.h"
};

int b32p3_rsave[] = {
	R1, R2, R3,
	R4, R5, R6, R7,
	R12,
	-1
};

int b32p3_rclob[] = {
	R8, R9, R10, R11,
	-1
};

#define RGLOB (BIT(R13) | BIT(R14) | BIT(R15))

static int
b32p3_memargs(int op)
{
	(void)op;
	return 0;
}

Target T_b32p3 = {
	.name = "b32p3",
	.gpr0 = R1,
	.km = Kw,
	.ngpr = NGPR,
	.fpr0 = 0,
	.nfpr = NFPR,
	.rglob = RGLOB,
	.nrglob = 3,
	.rsave = b32p3_rsave,
	.nrsave = {NGPS, NFPS},
	.retregs = b32p3_retregs,
	.argregs = b32p3_argregs,
	.memargs = b32p3_memargs,
	.abi0 = elimsb,
	.abi1 = b32p3_abi,
	.isel = b32p3_isel,
	.emitfn = b32p3_emitfn,
	.emitfin = b32p3_emitfin,
	.asloc = ".L",
};

MAKESURE(b32p3_rsave_ok, sizeof b32p3_rsave == (NGPS+NFPS+1) * sizeof(int));
MAKESURE(b32p3_rclob_ok, sizeof b32p3_rclob == (NCLR+1) * sizeof(int));
