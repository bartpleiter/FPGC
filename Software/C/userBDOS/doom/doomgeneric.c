#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	printf("DG_Create: saving args\n");
	// save arguments
    myargc = argc;
    myargv = argv;

	printf("DG_Create: M_FindResponseFile\n");
	M_FindResponseFile();

	printf("DG_Create: malloc screen buffer\n");
	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
	printf("DG_Create: buffer=%p\n", DG_ScreenBuffer);

	printf("DG_Create: DG_Init\n");
	DG_Init();

	printf("DG_Create: D_DoomMain\n");
	D_DoomMain ();
}

