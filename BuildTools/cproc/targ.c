#include <stdbool.h>
#include <string.h>
#include "util.h"
#include "cc.h"

const struct target *targ;

static const struct target alltargs[] = {
	{
		.name = "x86_64-sysv",
		.typewchar = &typeint,
		.typevalist = &(struct type){
			.kind = TYPEARRAY,
			.align = 8, .size = 24,
			.base = &(struct type){
				.kind = TYPESTRUCT,
				.align = 8, .size = 24,
			},
		},
		.signedchar = 1,
		.ptrsize = 8,
		.longsize = 8,
	},
	{
		.name = "aarch64",
		.typevalist = &(struct type){
			.kind = TYPESTRUCT,
			.align = 8, .size = 32,
			.u.structunion.tag = "va_list",
		},
		.typewchar = &typeuint,
		.ptrsize = 8,
		.longsize = 8,
	},
	{
		.name = "riscv64",
		.typevalist = &(struct type){
			.kind = TYPEPOINTER, .prop = PROPSCALAR,
			.align = 8, .size = 8,
			.base = &typevoid,
		},
		.typewchar = &typeint,
		.ptrsize = 8,
		.longsize = 8,
	},
	{
		.name = "b32p3",
		.typevalist = &(struct type){
			.kind = TYPEPOINTER, .prop = PROPSCALAR,
			.align = 4, .size = 4,
			.base = &typevoid,
		},
		.typewchar = &typeint,
		.signedchar = 1,
		.ptrsize = 4,
		.longsize = 4,
	},
};

void
targinit(const char *name)
{
	size_t i;
	enum typequal qual;

	if (!name) {
#ifdef __B32P3__
		name = "b32p3";
#else
		targ = &alltargs[0];
#endif
	}
	for (i = 0; i < LEN(alltargs) && !targ; ++i) {
		if (strcmp(alltargs[i].name, name) == 0)
			targ = &alltargs[i];
	}
	if (!targ)
		fatal("unknown target '%s'", name);

	/* Adjust type sizes for ILP32 targets */
	if (targ->ptrsize == 4) {
		typelong.size = 4;  typelong.align = 4;
		typeulong.size = 4; typeulong.align = 4;
		typellong.size = 4; typellong.align = 4;
		typeullong.size = 4; typeullong.align = 4;
		typenullptr.size = 4; typenullptr.align = 4;
	}

	typechar.u.basic.issigned = targ->signedchar;
	qual = QUALNONE;
	typeadjvalist = typeadjust(targ->typevalist, &qual);
}
