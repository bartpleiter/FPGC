/*
 * kernel.h — master include for the BDOS v4 kernel.
 *
 * All kernel .c files include this single header, which pulls in libc,
 * libfpgc (drivers, GPU, BRFS), and all kernel subsystem headers.
 */
#ifndef KERNEL_H
#define KERNEL_H

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
#include "term.h"
#include "ch376.h"
#include "enc28j60.h"
#include "brfs.h"
#include "sd.h"
#include "dma.h"

/* Kernel subsystem headers */
#include "mem.h"
#include "proc.h"
#include "vfs.h"
#include "dev.h"
#include "fs.h"
#include "syscall_nums.h"
#include "hid.h"
#include "net.h"

/* Core kernel functions (main.c) */
void kernel_panic(const char *msg);
void kernel_loop(void);
void kernel_log(const char *msg);

/* Initialization (init.c) */
void kernel_init(void);

#endif /* KERNEL_H */
