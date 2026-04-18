#ifndef BDOS_H
#define BDOS_H

/* Standard library (from libc) */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* FPGC hardware abstraction (from libfpgc) */
#include "fpgc.h"
#include "sys.h"
#include "uart.h"
#include "timer.h"
#include "spi.h"
#include "spi_flash.h"
#include "gpu_hal.h"
#include "gpu_fb.h"
#include "gpu_data_ascii.h"
#include "term2.h"
#include "ch376.h"
#include "enc28j60.h"
#include "brfs.h"

/* BDOS kernel modules */
#include "bdos_mem_map.h"
#include "bdos_heap.h"
#include "bdos_slot.h"
#include "bdos_syscall.h"
#include "bdos_hid.h"
#include "bdos_fs.h"
#include "bdos_fnp.h"
#include "bdos_shell.h"
#include "bdos_vfs.h"
#include "bdos_proc.h"

/* Core kernel functions (implemented in main.c) */
void bdos_panic(char *msg);
void bdos_loop(void);

/* Initialization (implemented in init.c) */
void bdos_init(void);

#endif /* BDOS_H */
