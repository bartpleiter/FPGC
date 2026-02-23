#ifndef BDOS_H
#define BDOS_H

// Domain headers
#include "BDOS/bdos_imports.h"
#include "BDOS/bdos_syscall.h"
#include "BDOS/bdos_hid.h"
#include "BDOS/bdos_fnp.h"
#include "BDOS/bdos_shell.h"
#include "BDOS/bdos_fs.h"
#include "BDOS/bdos_slot.h"

// Core BDOS functions
void bdos_panic(char* msg);
void bdos_init();
void bdos_loop();

#endif // BDOS_H
