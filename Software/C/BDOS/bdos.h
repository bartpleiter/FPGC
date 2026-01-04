#ifndef BDOS_H
#define BDOS_H

/*
 * BDOS main header file.
 */

// Include common libraries
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#include "libs/common/common.h"

// Include kernel libraries
#define KERNEL_GPU_HAL
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_TERM
#define KERNEL_UART
#define KERNEL_SPI
#define KERNEL_SPI_FLASH
#define KERNEL_BRFS
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

void bdos_panic(const char* msg);

#endif // BDOS_H
