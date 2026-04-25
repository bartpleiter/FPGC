#include <stdio.h>

#include "config.h"
#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	printf("DG_Create: argc=%d argv=%p\n", argc, argv);
	// save arguments
    myargc = argc;
    myargv = argv;

	/* Test: read back immediately to check if global write works */
	int readback_argc = myargc;
	char **readback_argv = myargv;
	printf("DG_Create: readback argc=%d argv=%p\n", readback_argc, readback_argv);
	printf("DG_Create: &myargc=%p &myargv=%p\n", &myargc, &myargv);

	printf("DG_Create: M_FindResponseFile\n");
	M_FindResponseFile();

	printf("DG_Create: malloc screen buffer\n");
	/* Allocate with 32-byte slack so we can hand DG_ScreenBuffer back
	 * 32-byte aligned — required by the DMA engine for MEM2VRAM blits
	 * (DG_DrawFrame uses dma_blit_to_vram). */
	{
		unsigned char *raw = (unsigned char *)malloc(
			DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(pixel_t) + 32);
		DG_ScreenBuffer = (pixel_t *)
			(((unsigned int)raw + 31u) & ~31u);
	}
	printf("DG_Create: buffer=%p\n", DG_ScreenBuffer);

	printf("DG_Create: DG_Init\n");
	DG_Init();

	printf("DG_Create: D_DoomMain\n");
	D_DoomMain ();
}

