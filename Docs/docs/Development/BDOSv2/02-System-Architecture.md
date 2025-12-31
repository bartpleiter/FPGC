# BDOS V2 System Architecture

**Prepared by: Dr. Sarah Chen (OS Architecture Lead)**  
**Contributors: Marcus Rodriguez, Dr. Emily Watson**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated for 64 MiB memory, PIC, software interrupt, raw Ethernet protocol*

---

## 1. Overview

This document describes the overall system architecture for BDOS V2, including component organization, interfaces, and data flow.

---

## 2. Architecture Style: Minimal Monolithic Kernel

### 2.1 Rationale

Given the constraints of the FPGC platform:

- **No MMU**: Cannot enforce memory protection between kernel and user space
- **No Linker**: Cannot create separate binaries and link at runtime
- **Simple Compiler**: Limited optimization and language features
- **Educational Focus**: Should use recognizable OS patterns

A **minimal monolithic kernel** is the most appropriate choice. This means:

1. All kernel code runs in the same address space
2. Modules communicate via function calls
3. Clear interfaces defined through header files
4. User programs run in dedicated memory regions with defined syscall interface

### 2.2 Comparison with Other Architectures

| Architecture | Pros | Cons | Fit for FPGC |
|--------------|------|------|--------------|
| **Monolithic** | Simple, fast, proven | Less modular | ✅ Best fit |
| **Microkernel** | Clean separation | IPC overhead, needs MMU | ❌ Too complex |
| **Exokernel** | Maximum flexibility | Complex, needs hardware support | ❌ Not suitable |
| **Library OS** | Minimal kernel | Application complexity | ⚠️ Possible |

---

## 3. System Layer Model

```
┌─────────────────────────────────────────────────────────────┐
│                     User Programs                           │
│  (Shell, Applications - run in dedicated memory slots)      │
├─────────────────────────────────────────────────────────────┤
│                   System Call Interface                     │
│              (Defined API for user programs)                │
├─────────────────────────────────────────────────────────────┤
│                    BDOS V2 Kernel                           │
│  ┌─────────┬──────────┬─────────┬──────────┬─────────────┐ │
│  │ Process │  Memory  │  File   │  Input   │   Network   │ │
│  │ Manager │ Manager  │ System  │ Subsys   │   Subsys    │ │
│  ├─────────┴──────────┴─────────┴──────────┴─────────────┤ │
│  │              Kernel Services Layer                     │ │
│  │  (Terminal, Scheduler, Timer Management, Buffers)      │ │
│  ├────────────────────────────────────────────────────────┤ │
│  │           Hardware Abstraction Layer (HAL)             │ │
│  │  (GPU, UART, SPI, Timers, Interrupts)                  │ │
│  └────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                     Hardware (FPGC)                         │
│  CPU | SDRAM | VRAM | SPI Flash | USB Hosts | Ethernet     │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Component Breakdown

### 4.1 Hardware Abstraction Layer (HAL)

**Location**: `Software/C/libs/kernel/hal/`

The HAL provides a consistent interface to hardware, hiding implementation details.

```
hal/
├── hal_gpu.h/c         # GPU/VRAM access
├── hal_uart.h/c        # UART communication
├── hal_spi.h/c         # SPI bus operations
├── hal_timer.h/c       # Timer configuration
├── hal_interrupt.h/c   # Interrupt enable/disable, ID
└── hal_io.h/c          # GPIO and misc I/O
```

**Design Principles**:
- Each HAL module is self-contained
- No global state where possible
- Consistent naming: `hal_<device>_<action>()`
- Returns standard error codes

**Example Interface**:
```c
// hal_uart.h
#ifndef HAL_UART_H
#define HAL_UART_H

#define HAL_UART_0  0  // USB UART
#define HAL_UART_1  1  // Header UART

void hal_uart_init(int uart_id);
int  hal_uart_putc(int uart_id, char c);
int  hal_uart_getc(int uart_id);  // Non-blocking, returns -1 if no data
int  hal_uart_write(int uart_id, const char* buf, int len);

#endif
```

### 4.2 Kernel Services Layer

**Location**: `Software/C/BDOS/kernel/services/`

Core services used by all other kernel components.

```
services/
├── kprintf.h/c         # Kernel debug output
├── kmalloc.h/c         # Kernel memory allocation
├── kstring.h/c         # String utilities for kernel
├── kbuffer.h/c         # Ring buffer implementation
└── ktimer.h/c          # Timer management
```

### 4.3 Subsystem Modules

Each major subsystem is a separate module:

| Module | Location | Responsibility |
|--------|----------|----------------|
| **Process Manager** | `kernel/proc/` | Program slots, switching, state |
| **Memory Manager** | `kernel/mem/` | Heap, program loading |
| **File System** | `kernel/fs/` | BRFS operations, VFS abstraction |
| **Input Subsystem** | `kernel/input/` | Keyboard, input event queue |
| **Output Subsystem** | `kernel/output/` | Terminal, streams |
| **Network Subsystem** | `kernel/net/` | Ethernet driver, raw frame protocol |
| **System Calls** | `kernel/syscall/` | Software interrupt interface |

---

## 5. Boot Sequence

### 5.1 Boot Flow

```
┌──────────────────────────────────────────────────────────────┐
│ 1. ROM Bootloader (0x7800000)                                │
│    - Initialize CPU                                          │
│    - Check boot mode switch                                  │
│    - Load BDOS from SPI Flash or UART                       │
│    - Jump to BDOS entry point                               │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 2. BDOS Kernel Init (bdos_init)                              │
│    - Initialize HAL (GPIO, timers, UART)                    │
│    - Initialize kernel heap                                  │
│    - Initialize terminal (GPU/VRAM)                         │
│    - Print boot messages                                     │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. Subsystem Initialization                                   │
│    - Initialize process manager                              │
│    - Initialize file system (mount BRFS from flash)         │
│    - Initialize input subsystem                              │
│    - Initialize network subsystem                            │
│    - Initialize system call handler                          │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 4. Shell Startup                                              │
│    - Initialize shell state                                  │
│    - Display prompt                                          │
│    - Enter main loop                                         │
└──────────────────────────────────────────────────────────────┘
```

### 5.2 Main Loop

The kernel main loop is event-driven, processing inputs and running background tasks:

```c
void bdos_main_loop(void) {
    while (1) {
        // 1. Process input (keyboard, network HID)
        input_poll();
        
        // 2. Handle network events
        net_poll();
        
        // 3. Run active program (if any)
        if (proc_has_active()) {
            proc_run_active();
        } else {
            // 4. Run shell if no program active
            shell_update();
        }
        
        // 5. Run background tasks (if any)
        sched_run_background();
    }
}
```

---

## 6. Interrupt Architecture

### 6.1 Interrupt Sources

| ID | Source | Handler | Priority |
|----|--------|---------|----------|
| 1 | UART0 | Debug input | Low |
| 2 | Timer0 | System tick | Medium |
| 3 | Timer1 | User timer | Low |
| 4 | Timer2 | USB keyboard polling | High |
| 5 | Frame Drawn | GPU sync | Low |

### 6.2 Interrupt Handling Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    Interrupt Occurs                          │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 1. CPU saves PC, disables interrupts                         │
│ 2. Jump to interrupt() function                              │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 3. Kernel interrupt handler                                  │
│    a. Read interrupt ID                                      │
│    b. Dispatch to specific handler                          │
│    c. Handle kernel-level interrupts                        │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 4. User program callback (if registered)                     │
│    - Save kernel registers                                   │
│    - Call user interrupt handler                            │
│    - Restore kernel registers                               │
└─────────────────────┬────────────────────────────────────────┘
                      ▼
┌──────────────────────────────────────────────────────────────┐
│ 5. Return from interrupt (RETI)                              │
│    - Restore PC                                             │
│    - Re-enable interrupts                                   │
└──────────────────────────────────────────────────────────────┘
```

### 6.3 User Program Interrupt Handling (Improved Design)

The old BDOS used inline assembly to jump to a fixed address for user interrupts. BDOS V2 improves this:

**Option A: Callback Registration (Recommended)**

```c
// In kernel
typedef void (*interrupt_callback_t)(int int_id);
interrupt_callback_t user_int_callbacks[NUM_INTERRUPTS];

void syscall_register_interrupt(int int_id, interrupt_callback_t callback) {
    if (int_id >= 0 && int_id < NUM_INTERRUPTS) {
        user_int_callbacks[int_id] = callback;
    }
}

// In interrupt handler
void interrupt(void) {
    int id = hal_interrupt_get_id();
    
    // Kernel handling first
    kernel_handle_interrupt(id);
    
    // User callback if registered
    if (user_int_callbacks[id] != NULL) {
        user_int_callbacks[id](id);
    }
}
```

**Option B: Fixed Entry Point with Dispatch Table**

User program provides a dispatch table at a known offset:

```c
// User program structure at start of program slot
struct user_program_header {
    void (*entry)(void);           // Main entry point
    void (*interrupt)(int id);     // Interrupt handler
    void (*cleanup)(void);         // Cleanup before exit
};
```

---

## 7. Directory Structure

### 7.1 Proposed Source Layout

```
Software/C/BDOS/
├── bdos.c                 # Main entry point
├── bdos.h                 # Main header with config
├── Makefile               # Build configuration
│
├── kernel/                # Kernel subsystems
│   ├── core/              # Core kernel services
│   │   ├── init.c/h       # Initialization
│   │   ├── panic.c/h      # Kernel panic handling
│   │   └── debug.c/h      # Debug utilities
│   │
│   ├── proc/              # Process management
│   │   ├── process.c/h    # Process state
│   │   ├── switch.c/h     # Context switching
│   │   └── loader.c/h     # Program loading
│   │
│   ├── mem/               # Memory management
│   │   ├── heap.c/h       # Heap allocator
│   │   └── regions.c/h    # Memory region management
│   │
│   ├── fs/                # File system
│   │   ├── vfs.c/h        # Virtual file system layer
│   │   ├── brfs.c/h       # BRFS implementation
│   │   └── file.c/h       # File handle management
│   │
│   ├── input/             # Input subsystem
│   │   ├── input.c/h      # Input manager
│   │   ├── kbd_usb.c/h    # USB keyboard driver
│   │   └── kbd_net.c/h    # Network HID
│   │
│   ├── output/            # Output subsystem
│   │   ├── terminal.c/h   # Terminal emulation
│   │   └── console.c/h    # Console output
│   │
│   ├── net/               # Network subsystem
│   │   ├── enc28j60.c/h   # Ethernet driver
│   │   ├── protocol.c/h   # Custom protocol
│   │   └── netloader.c/h  # Program loading via network
│   │
│   ├── syscall/           # System calls
│   │   ├── syscall.c/h    # Syscall dispatcher
│   │   └── handlers.c/h   # Individual handlers
│   │
│   └── sched/             # Scheduler (simple)
│       └── background.c/h # Background task runner
│
├── shell/                 # Shell implementation
│   ├── shell.c/h          # Main shell logic
│   ├── commands.c/h       # Built-in commands
│   └── parser.c/h         # Command parsing
│
├── hal/                   # Hardware abstraction (or use libs/kernel/)
│   └── (see section 4.1)
│
└── include/               # Public headers for user programs
    ├── bdos.h             # Main user API
    ├── syscall.h          # System call numbers
    ├── types.h            # Type definitions
    └── errno.h            # Error codes
```

### 7.2 Integration with Existing libs/

The existing `Software/C/libs/kernel/` already contains good abstractions. BDOS V2 should:

1. **Reuse**: Use existing GPU, UART, SPI, Timer, BRFS implementations
2. **Extend**: Add BDOS-specific wrappers where needed
3. **Refactor**: Move BDOS-specific code out of generic libs

```c
// bdos.c - Using existing libs
#define COMMON_STDLIB
#define COMMON_STRING
#include "libs/common/common.h"

#define KERNEL_GPU_HAL
#define KERNEL_GPU_FB
#define KERNEL_TERM
#define KERNEL_BRFS
#define KERNEL_SPI_FLASH
#define KERNEL_TIMER
#include "libs/kernel/kernel.h"

// Additional BDOS-specific includes
#include "kernel/proc/process.h"
#include "kernel/syscall/syscall.h"
// ...
```

---

## 8. Error Handling Strategy

### 8.1 Error Codes

Use consistent error codes across all modules:

```c
// include/errno.h
#define EOK         0       // Success
#define EERROR      -1      // Generic error
#define ENOMEM      -2      // Out of memory
#define ENOENT      -3      // No such file/entry
#define EEXIST      -4      // Already exists
#define EINVAL      -5      // Invalid argument
#define EIO         -6      // I/O error
#define EBUSY       -7      // Resource busy
#define ENOSYS      -8      // Function not implemented
#define EPERM       -9      // Permission denied
#define EAGAIN      -10     // Try again
```

### 8.2 Kernel Panic

For unrecoverable errors:

```c
void kernel_panic(const char* message) {
    // Disable interrupts
    hal_interrupt_disable_all();
    
    // Print panic message
    term_clear();
    term_puts("*** KERNEL PANIC ***\n");
    term_puts(message);
    term_puts("\n\nSystem halted.");
    
    // Halt
    while (1) {
        asm("halt");
    }
}
```

---

## 9. Configuration

### 9.1 Compile-Time Configuration

```c
// bdos.h - Configuration section

// Memory configuration (64 MiB physical device)
#define BDOS_KERNEL_START       0x0000000
#define BDOS_KERNEL_SIZE        0x0080000   // 512 KiW (2 MiB)
#define BDOS_HEAP_START         0x0080000
#define BDOS_HEAP_SIZE          0x0080000   // 512 KiW (2 MiB)
#define BDOS_PROGRAM_START      0x0100000
#define BDOS_PROGRAM_END        0x0800000
#define BDOS_SLOT_SIZE          0x0080000   // 512 KiW each
#define BDOS_MAX_SLOTS          14          // 7 MiW / 512 KiW
#define BDOS_BRFS_START         0x0800000   // Last 32 MiB

// Feature flags
#define BDOS_ENABLE_NETWORK     1
#define BDOS_ENABLE_USB_KBD     1
#define BDOS_ENABLE_DEBUG_UART  1
#define BDOS_ENABLE_BACKGROUND  1

// Limits
#define BDOS_MAX_OPEN_FILES     16
#define BDOS_INPUT_BUFFER_SIZE  64
#define BDOS_MAX_PATH_LENGTH    128
```

---

## 10. Component Interfaces (Summary)

| Interface | Functions | Purpose |
|-----------|-----------|---------|
| `proc_*` | init, create, switch, terminate, get_current | Process management |
| `mem_*` | init, alloc, free, load_program | Memory management |
| `fs_*` | init, open, close, read, write, seek, stat | File operations |
| `input_*` | init, poll, read, register_callback | Input handling |
| `term_*` | init, putc, puts, clear, scroll | Terminal output |
| `net_*` | init, poll, send, receive | Network operations |
| `syscall_*` | init, dispatch | System call handling |

---

## 11. Design Alternatives

### Alternative A: Event-Driven vs Polling

**Current Recommendation**: Hybrid (polling main loop with interrupt-driven input)

**Alternative**: Full event-driven with sleep
- Would require implementing sleep/wake mechanism
- More complex but more power-efficient
- Consider for future versions

### Alternative B: Single Binary vs Module Loading

**Current Recommendation**: Single binary kernel

**Alternative**: Load kernel modules from filesystem
- Would allow updating components without full recompile
- Requires implementing a simple loader
- Complex without linker support
- Consider for future versions if codebase grows large

---

## 12. Next Steps

After architecture approval:

1. Set up directory structure
2. Implement HAL layer (can reuse existing libs)
3. Implement kernel core (init, panic, debug)
4. Implement basic terminal output
5. Test boot sequence

See **Implementation Guide** (Document 10) for detailed code examples.
