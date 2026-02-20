#ifndef KERNEL_H
#define KERNEL_H

/*
 * Kernel Library Orchestrator
 * Handles inclusion of kernel modules.
 * Allows inclusion of only needed libraries without needing a linker.
 */

// Dependency resolution
#ifdef KERNEL_GPU_FB
#ifndef KERNEL_GPU_HAL
#define KERNEL_GPU_HAL
#endif
#endif

#ifdef KERNEL_TERM
#ifndef KERNEL_GPU_HAL
#define KERNEL_GPU_HAL
#endif
#endif

#ifdef KERNEL_BRFS
#ifndef KERNEL_SPI_FLASH
#define KERNEL_SPI_FLASH
#endif
#endif

#ifdef KERNEL_SPI_FLASH
#ifndef KERNEL_SPI
#define KERNEL_SPI
#endif
#endif

#ifdef KERNEL_CH376
#ifndef KERNEL_SPI
#define KERNEL_SPI
#endif
#endif

#ifdef KERNEL_ENC28J60
#ifndef KERNEL_SPI
#define KERNEL_SPI
#endif
#ifndef KERNEL_TIMER
#define KERNEL_TIMER
#endif
#endif

#ifdef KERNEL_MEM_DEBUG
#ifndef KERNEL_UART
#define KERNEL_UART
#endif
#endif

// Flag based inclusion of libraries
// Header files
#ifdef KERNEL_GPU_HAL
#include "libs/kernel/gfx/gpu_hal.h"
#endif

#ifdef KERNEL_GPU_FB
#include "libs/kernel/gfx/gpu_fb.h"
#endif

#ifdef KERNEL_GPU_DATA_ASCII
#include "libs/kernel/gfx/gpu_data_ascii.h"
#endif

#ifdef KERNEL_UART
#include "libs/kernel/io/uart.h"
#endif

#ifdef KERNEL_TERM
#include "libs/kernel/term/term.h"
#endif

#ifdef KERNEL_SPI
#include "libs/kernel/io/spi.h"
#endif

#ifdef KERNEL_MALLOC
#include "libs/kernel/mem/malloc.h"
#endif

#ifdef KERNEL_SPI_FLASH
#include "libs/kernel/io/spi_flash.h"
#endif

#ifdef KERNEL_CH376
#include "libs/kernel/io/ch376.h"
#endif

#ifdef KERNEL_ENC28J60
#include "libs/kernel/io/enc28j60.h"
#endif

#ifdef KERNEL_BRFS
#include "libs/kernel/fs/brfs.h"
#endif

#ifdef KERNEL_MEM_DEBUG
#include "libs/kernel/mem/debug.h"
#endif

#ifdef KERNEL_TIMER
#include "libs/kernel/io/timer.h"
#endif

// Always include sys library
#include "libs/kernel/sys.h"

// Implementation files
#ifdef KERNEL_GPU_HAL
#include "libs/kernel/gfx/gpu_hal.c"
#endif

#ifdef KERNEL_GPU_FB
#include "libs/kernel/gfx/gpu_fb.c"
#endif

#ifdef KERNEL_GPU_DATA_ASCII
#include "libs/kernel/gfx/gpu_data_ascii.c"
#endif

#ifdef KERNEL_UART
#include "libs/kernel/io/uart.c"
#endif

#ifdef KERNEL_TERM
#include "libs/kernel/term/term.c"
#endif

#ifdef KERNEL_SPI
#include "libs/kernel/io/spi.c"
#endif

#ifdef KERNEL_MALLOC
#include "libs/kernel/mem/malloc.c"
#endif

#ifdef KERNEL_SPI_FLASH
#include "libs/kernel/io/spi_flash.c"
#endif

#ifdef KERNEL_CH376
#include "libs/kernel/io/ch376.c"
#endif

#ifdef KERNEL_ENC28J60
#include "libs/kernel/io/enc28j60.c"
#endif

#ifdef KERNEL_BRFS
#include "libs/kernel/fs/brfs.c"
#endif

#ifdef KERNEL_MEM_DEBUG
#include "libs/kernel/mem/debug.c"
#endif

#ifdef KERNEL_TIMER
#include "libs/kernel/io/timer.c"
#endif

// Always include sys library
#include "libs/kernel/sys.c"

#endif // KERNEL_H
