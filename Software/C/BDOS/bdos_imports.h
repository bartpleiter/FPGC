#ifndef BDOS_IMPORTS_H
#define BDOS_IMPORTS_H

// Common libraries
#define COMMON_STRING
#define COMMON_STDLIB
#define COMMON_CTYPE
#define COMMON_TIME
#include "libs/common/common.h"

// Kernel libraries
#define KERNEL_GPU_HAL
#define KERNEL_GPU_FB
#define KERNEL_GPU_DATA_ASCII
#define KERNEL_TERM
#define KERNEL_UART
#define KERNEL_SPI
#define KERNEL_SPI_FLASH
#define KERNEL_BRFS
#define KERNEL_TIMER
#define KERNEL_CH376
#define KERNEL_ENC28J60
#include "libs/kernel/kernel.h"

// Memory map definitions
#include "BDOS/mem_map.h"

#endif // BDOS_IMPORTS_H
