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

#ifdef KERNEL_TERM
#include "libs/kernel/term/term.h"
#endif

#ifdef KERNEL_SPI
#include "libs/kernel/io/spi.h"
#endif

#ifdef KERNEL_MALLOC
#include "libs/kernel/mem/malloc.h"
#endif

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

#ifdef KERNEL_TERM
#include "libs/kernel/term/term.c"
#endif

#ifdef KERNEL_SPI
#include "libs/kernel/io/spi.c"
#endif

#ifdef KERNEL_MALLOC
#include "libs/kernel/mem/malloc.c"
#endif

#endif // KERNEL_H
