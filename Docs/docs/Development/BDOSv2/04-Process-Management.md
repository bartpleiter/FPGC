# BDOS V2 Process Management

**Prepared by: Dr. Sarah Chen (OS Architecture Lead)**  
**Contributors: Marcus Rodriguez**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated for 14 slots, 512 KiW slot size, hardware context save discussion*

---

## 1. Overview

This document describes the process management subsystem for BDOS V2, including process states, context switching, the "alt-tab" program switching feature, and background task support.

---

## 2. Process Model

### 2.1 What is a "Process" in BDOS V2?

Given the hardware constraints (no MMU, no preemption by default), a "process" in BDOS V2 is simpler than in traditional operating systems:

- A **process** is a loaded program in a memory slot
- Only one process executes at a time (foreground)
- Processes can be suspended and resumed
- No true concurrent execution (except interrupt-driven background tasks)

### 2.2 Process Control Block (PCB)

```c
// kernel/proc/process.h

#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES   14      // Matches slot count (14 × 512 KiW)
#define PROC_NAME_LEN   16

// Process states
typedef enum {
    PROC_STATE_EMPTY = 0,       // Slot is empty
    PROC_STATE_READY,           // Loaded, ready to run
    PROC_STATE_RUNNING,         // Currently executing
    PROC_STATE_SUSPENDED,       // Suspended (alt-tabbed away)
    PROC_STATE_BLOCKED,         // Waiting for I/O (future)
    PROC_STATE_TERMINATED       // Finished, awaiting cleanup
} proc_state_t;

// Process Control Block
struct process {
    // Identity
    unsigned int pid;           // Process ID (1-based, 0 = none)
    char name[PROC_NAME_LEN];   // Process name (from filename)
    
    // State
    proc_state_t state;         // Current state
    int exit_code;              // Exit code when terminated
    
    // Memory
    int first_slot;             // First slot number (0-13)
    int slot_count;             // Number of slots used (1+ for large programs)
    unsigned int load_addr;     // Base address where loaded
    unsigned int size;          // Total size in words
    
    // Execution context (saved when suspended)
    unsigned int saved_regs[16];// r0-r15 (r0 ignored)
    unsigned int saved_pc;      // Program counter
    unsigned int saved_sp;      // Stack pointer (r13)
    
    // Resources
    int open_files[8];          // File descriptors (index into global table)
    unsigned int int_handlers[8]; // Registered interrupt handlers
    
    // Statistics (optional)
    unsigned int start_time;    // When process started (micros)
    unsigned int cpu_time;      // Accumulated CPU time (future)
};

// Process table
extern struct process proc_table[MAX_PROCESSES];
extern int current_pid;  // Currently running process (0 = kernel/shell)

// Process management functions
void proc_init(void);
int  proc_create(const char* path, int argc, char** argv);
int  proc_terminate(int pid, int exit_code);
int  proc_suspend(int pid);
int  proc_resume(int pid);
int  proc_switch(int from_pid, int to_pid);
struct process* proc_get(int pid);
struct process* proc_get_current(void);
int  proc_get_current_pid(void);

#endif // PROCESS_H
```

### 2.3 Process Lifecycle

```
┌─────────────────────────────────────────────────────────────────────┐
│                         PROCESS LIFECYCLE                           │
└─────────────────────────────────────────────────────────────────────┘

                    proc_create()
       ┌─────────────────────────────────┐
       │                                 │
       ▼                                 │
  ┌─────────┐                            │
  │  EMPTY  │◄───────────────────────────┼─────────────────┐
  └────┬────┘                            │                 │
       │ load_program()                  │                 │
       ▼                                 │                 │
  ┌─────────┐                            │                 │
  │  READY  │◄───────────┐               │                 │
  └────┬────┘            │               │                 │
       │ schedule()      │ I/O complete  │                 │
       ▼                 │ (future)      │                 │
  ┌─────────┐       ┌────┴────┐          │                 │
  │ RUNNING │◄─────►│ BLOCKED │          │                 │
  └────┬────┘       └─────────┘          │                 │
       │                                 │                 │
       ├─────────────────────────────────┤                 │
       │ proc_suspend()                  │                 │
       ▼                                 │                 │
  ┌───────────┐                          │                 │
  │ SUSPENDED │──────────────────────────┘                 │
  └─────┬─────┘   proc_resume()                            │
        │                                                  │
        │ exit() or return from main()                     │
        ▼                                                  │
  ┌────────────┐                                           │
  │ TERMINATED │───────────────────────────────────────────┘
  └────────────┘   proc_cleanup()
```

---

## 3. Context Switching

### 3.1 What Needs to Be Saved

When switching between processes, the following must be saved/restored:

| Item | Size | Notes |
|------|------|-------|
| Registers r1-r15 | 15 words | r0 is always 0 |
| Program Counter | 1 word | Where to resume |
| Stack Pointer | 1 word | Also r13, but saved explicitly |
| Interrupt state | 1 word | Which handlers registered |

**Total: ~18 words per context**

### 3.2 Context Switch Implementation

```c
// kernel/proc/context.h

#ifndef CONTEXT_H
#define CONTEXT_H

struct context {
    unsigned int regs[16];  // r0-r15
    unsigned int pc;        // Program counter
};

// Save current context to structure
void context_save(struct context* ctx);

// Restore context from structure
void context_restore(struct context* ctx);

// Switch from one context to another
void context_switch(struct context* old_ctx, struct context* new_ctx);

#endif // CONTEXT_H
```

```c
// kernel/proc/context.c

// These must be implemented in assembly for correctness

void context_save(struct context* ctx) {
    // Save all registers to ctx->regs
    // Note: This function modifies registers, so must save before using them
    asm(
        "; r4 contains ctx pointer (first argument)\n"
        "write 0 r4 r0\n"   // ctx->regs[0] = r0 (always 0, but consistent)
        "write 1 r4 r1\n"   // ctx->regs[1] = r1
        "write 2 r4 r2\n"   // ctx->regs[2] = r2
        "write 3 r4 r3\n"   // ctx->regs[3] = r3
        "write 4 r4 r4\n"   // ctx->regs[4] = r4 (self-reference, gets overwritten)
        "write 5 r4 r5\n"
        "write 6 r4 r6\n"
        "write 7 r4 r7\n"
        "write 8 r4 r8\n"
        "write 9 r4 r9\n"
        "write 10 r4 r10\n"
        "write 11 r4 r11\n"
        "write 12 r4 r12\n"
        "write 13 r4 r13\n" // Stack pointer
        "write 14 r4 r14\n" // Frame pointer
        "write 15 r4 r15\n" // Link register / return address
        
        "; Save PC (caller's return address is in r15 typically)\n"
        "savpc r1\n"
        "write 16 r4 r1\n"  // ctx->pc = current PC
    );
}

void context_restore(struct context* ctx) {
    asm(
        "; r4 contains ctx pointer\n"
        "; Restore registers in reverse order to avoid clobbering\n"
        "read 16 r4 r1\n"   // r1 = ctx->pc (will jump to this)
        "push r1\n"         // Save PC for later jump
        
        "read 15 r4 r15\n"
        "read 14 r4 r14\n"
        "read 13 r4 r13\n"  // Stack pointer
        "read 12 r4 r12\n"
        "read 11 r4 r11\n"
        "read 10 r4 r10\n"
        "read 9 r4 r9\n"
        "read 8 r4 r8\n"
        "read 7 r4 r7\n"
        "read 6 r4 r6\n"
        "read 5 r4 r5\n"
        "read 3 r4 r3\n"
        "read 2 r4 r2\n"
        "read 1 r4 r1\n"
        "read 4 r4 r4\n"    // Finally restore r4 (ctx pointer)
        
        "; Jump to saved PC\n"
        "pop r0\n"          // Actually pop to temp (r0 discarded)
        "jumpr r0\n"        // Jump to restored PC
    );
}
```

### 3.3 The Full Switch Operation

```c
// kernel/proc/switch.c

#include "process.h"
#include "context.h"
#include "kernel/mem/slot.h"

int proc_switch(int from_pid, int to_pid) {
    struct process* from_proc = proc_get(from_pid);
    struct process* to_proc = proc_get(to_pid);
    
    if (from_proc == NULL || to_proc == NULL) {
        return -EINVAL;
    }
    
    if (to_proc->state != PROC_STATE_READY && 
        to_proc->state != PROC_STATE_SUSPENDED) {
        return -EINVAL;
    }
    
    // 1. Save current context
    if (from_proc->state == PROC_STATE_RUNNING) {
        context_save((struct context*)&from_proc->saved_regs);
        from_proc->state = PROC_STATE_SUSPENDED;
    }
    
    // 2. If processes are in different slots, copy to active slot
    if (to_proc->slot != SLOT_ACTIVE) {
        // First, save active slot to from_proc's storage slot
        if (from_proc != NULL && from_proc->slot == SLOT_ACTIVE) {
            int storage_slot = slot_find_empty();
            if (storage_slot < 0) {
                // Need to evict someone - for now, error
                return -ENOMEM;
            }
            slot_copy(SLOT_ACTIVE, storage_slot, from_proc->size);
            from_proc->slot = storage_slot;
        }
        
        // Copy to_proc to active slot
        slot_copy(to_proc->slot, SLOT_ACTIVE, to_proc->size);
        slot_clear(to_proc->slot);
        to_proc->slot = SLOT_ACTIVE;
    }
    
    // 3. Update state
    to_proc->state = PROC_STATE_RUNNING;
    current_pid = to_pid;
    
    // 4. Restore context and jump to process
    context_restore((struct context*)&to_proc->saved_regs);
    
    // Never reaches here (context_restore jumps away)
    return EOK;
}
```

---

## 4. Program Switching ("Alt-Tab")

### 4.1 User Interface Design

| Option | Trigger | UX | Recommendation |
|--------|---------|-----|----------------|
| **Hotkey** | Ctrl+Tab or special key | Instant, familiar | ✅ Primary |
| **Command** | `switch` in shell | Type command | Backup method |
| **Network** | Network command | Remote control | For netHID |

### 4.2 Hotkey Implementation

The input subsystem watches for a special key combination:

```c
// kernel/input/hotkey.c

#define HOTKEY_SWITCH   0x09    // Tab key
#define HOTKEY_MOD_CTRL 0x01    // Ctrl modifier

int hotkey_check(unsigned int keycode, unsigned int modifiers) {
    // Check for Ctrl+Tab
    if (keycode == HOTKEY_SWITCH && (modifiers & HOTKEY_MOD_CTRL)) {
        return HOTKEY_ACTION_SWITCH;
    }
    return HOTKEY_ACTION_NONE;
}
```

### 4.3 Process Switcher

```c
// kernel/proc/switcher.c

#include "process.h"

// Circular list of active processes
static int proc_ring[MAX_PROCESSES];
static int proc_ring_count = 0;
static int proc_ring_current = 0;

void switcher_add(int pid) {
    if (proc_ring_count < MAX_PROCESSES) {
        proc_ring[proc_ring_count++] = pid;
    }
}

void switcher_remove(int pid) {
    for (int i = 0; i < proc_ring_count; i++) {
        if (proc_ring[i] == pid) {
            // Shift remaining elements
            for (int j = i; j < proc_ring_count - 1; j++) {
                proc_ring[j] = proc_ring[j + 1];
            }
            proc_ring_count--;
            if (proc_ring_current >= proc_ring_count) {
                proc_ring_current = 0;
            }
            return;
        }
    }
}

// Called when Ctrl+Tab pressed
void switcher_next(void) {
    if (proc_ring_count <= 1) {
        // No other process to switch to
        // Maybe show message or beep
        return;
    }
    
    // Move to next process in ring
    int old_pid = proc_ring[proc_ring_current];
    proc_ring_current = (proc_ring_current + 1) % proc_ring_count;
    int new_pid = proc_ring[proc_ring_current];
    
    // Display switch indicator (optional)
    term_puts("\n[Switching to: ");
    term_puts(proc_get(new_pid)->name);
    term_puts("]\n");
    
    // Perform the switch
    proc_switch(old_pid, new_pid);
}
```

### 4.4 Visual Feedback (Optional)

When switching, briefly show which program is being switched to:

```c
void show_switch_overlay(int to_pid) {
    struct process* proc = proc_get(to_pid);
    
    // Save current cursor position
    unsigned int old_x, old_y;
    term_get_cursor(&old_x, &old_y);
    
    // Draw overlay at top of screen
    term_set_cursor(0, 0);
    term_set_palette(PALETTE_HIGHLIGHT);
    term_puts("[ Switching to: ");
    term_puts(proc->name);
    term_puts(" ]");
    term_set_palette(PALETTE_DEFAULT);
    
    // Small delay so user sees it
    delay_ms(200);
    
    // Restore cursor (actual screen will be replaced anyway)
    term_set_cursor(old_x, old_y);
}
```

---

## 5. Background Tasks

### 5.1 Design Philosophy

True preemptive multitasking is complex without hardware support. Instead, BDOS V2 supports:

1. **Interrupt-driven background tasks**: Small functions that run in interrupt context
2. **Cooperative background tasks**: Called from main loop when foreground is idle

### 5.2 Interrupt-Driven Tasks

These are lightweight tasks that run within interrupt handlers:

```c
// Already implemented in old BDOS:
// - USB keyboard polling (Timer2 interrupt)
// - Network HID checking (GPU frame interrupt)

// kernel/sched/background.h

typedef void (*bg_task_fn)(void);

#define MAX_BG_TASKS    8

struct bg_task {
    bg_task_fn function;    // Function to call
    unsigned int interval;  // Minimum ms between calls
    unsigned int last_run;  // Last execution time
    int enabled;            // Is task active?
};

void bg_init(void);
int  bg_register(bg_task_fn fn, unsigned int interval_ms);
void bg_unregister(bg_task_fn fn);
void bg_enable(bg_task_fn fn);
void bg_disable(bg_task_fn fn);
void bg_tick(void);  // Called from timer interrupt
```

### 5.3 Cooperative Tasks

These run in the main loop during idle time:

```c
// kernel/sched/coop.c

#define MAX_COOP_TASKS  8

struct coop_task {
    bg_task_fn function;
    int enabled;
};

static struct coop_task coop_tasks[MAX_COOP_TASKS];

void coop_run_pending(void) {
    for (int i = 0; i < MAX_COOP_TASKS; i++) {
        if (coop_tasks[i].enabled && coop_tasks[i].function != NULL) {
            coop_tasks[i].function();
        }
    }
}
```

### 5.4 Example: Network Polling as Background Task

```c
// Register network polling as cooperative background task
void net_init(void) {
    // ... network initialization ...
    
    // Register polling function
    coop_register(net_poll);
}

void net_poll(void) {
    // Check for incoming packets
    // Handle network loader requests
    // Handle network HID input
    // This runs even when user program is in foreground
}
```

### 5.5 Constraints on Background Tasks

| Type | Can Access | Cannot Access | Max Duration |
|------|------------|---------------|--------------|
| Interrupt | Kernel data, I/O | User memory, heap | ~100 µs |
| Cooperative | Everything | N/A | ~10 ms |

---

## 6. Process Creation Flow

### 6.1 From Shell Command

```
User types: /bin/program arg1 arg2

┌────────────────────────────────────────────────────────────────────┐
│ 1. Shell parses command                                            │
│    - Extract program path: "/bin/program"                          │
│    - Extract arguments: ["arg1", "arg2"]                           │
└───────────────────────────┬────────────────────────────────────────┘
                            ▼
┌────────────────────────────────────────────────────────────────────┐
│ 2. proc_create("/bin/program", argc, argv)                         │
│    - Find empty PCB slot                                           │
│    - Find empty memory slot                                        │
│    - Load program from filesystem                                  │
│    - Set up stack with arguments                                   │
│    - Initialize PCB                                                │
│    - Return new PID                                                │
└───────────────────────────┬────────────────────────────────────────┘
                            ▼
┌────────────────────────────────────────────────────────────────────┐
│ 3. Switch to new process                                           │
│    - Shell state saved (or shell is part of kernel)                │
│    - Load new process context                                      │
│    - Jump to entry point                                           │
└────────────────────────────────────────────────────────────────────┘
```

### 6.2 Detailed proc_create() Implementation

```c
// kernel/proc/create.c

int proc_create(const char* path, int argc, char** argv) {
    // 1. Find empty PCB
    int pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_STATE_EMPTY) {
            pid = i + 1;  // PIDs are 1-based
            break;
        }
    }
    if (pid < 0) {
        return -EAGAIN;  // No free PCB
    }
    
    struct process* proc = &proc_table[pid - 1];
    
    // 2. Find empty memory slot
    int slot = slot_find_empty();
    if (slot < 0) {
        return -ENOMEM;
    }
    
    // 3. Load program to slot
    int result = load_program(path, slot);
    if (result < 0) {
        return result;
    }
    
    // 4. Initialize PCB
    proc->pid = pid;
    strncpy(proc->name, basename(path), PROC_NAME_LEN - 1);
    proc->state = PROC_STATE_READY;
    proc->slot = slot;
    proc->load_addr = (unsigned int)slot_get_base(slot);
    
    // Get size from program header
    struct program_header* hdr = (struct program_header*)proc->load_addr;
    proc->size = PROGRAM_HEADER_SIZE + hdr->code_size + 
                 hdr->data_size + hdr->bss_size;
    
    // 5. Set up initial context
    memset(proc->saved_regs, 0, sizeof(proc->saved_regs));
    
    // Stack at top of slot
    unsigned int* stack_top = slot_get_base(slot) + MEM_PROGRAM_SLOT_SIZE - 1;
    
    // Push arguments onto stack (argc, argv)
    // (Detailed implementation depends on calling convention)
    stack_top = setup_args(stack_top, argc, argv);
    
    proc->saved_regs[13] = (unsigned int)stack_top;  // SP
    proc->saved_pc = proc->load_addr + hdr->entry_offset;
    
    // 6. Initialize resource tracking
    memset(proc->open_files, -1, sizeof(proc->open_files));
    memset(proc->int_handlers, 0, sizeof(proc->int_handlers));
    
    proc->start_time = get_micros();
    
    // 7. Add to process ring
    switcher_add(pid);
    
    return pid;
}
```

---

## 7. Process Termination

### 7.1 Normal Exit

```c
// Called when program returns from main() or calls exit()
void proc_exit(int exit_code) {
    int pid = proc_get_current_pid();
    proc_terminate(pid, exit_code);
}

int proc_terminate(int pid, int exit_code) {
    struct process* proc = proc_get(pid);
    if (proc == NULL) {
        return -EINVAL;
    }
    
    // 1. Update state
    proc->state = PROC_STATE_TERMINATED;
    proc->exit_code = exit_code;
    
    // 2. Close all open files
    for (int i = 0; i < 8; i++) {
        if (proc->open_files[i] >= 0) {
            fs_close(proc->open_files[i]);
        }
    }
    
    // 3. Unregister interrupt handlers
    for (int i = 0; i < 8; i++) {
        if (proc->int_handlers[i] != 0) {
            int_unregister(i, proc->int_handlers[i]);
        }
    }
    
    // 4. Remove from process ring
    switcher_remove(pid);
    
    // 5. Free memory slot
    slot_clear(proc->slot);
    
    // 6. If this was the running process, switch to another
    if (pid == current_pid) {
        if (proc_ring_count > 0) {
            proc_switch(pid, proc_ring[0]);
        } else {
            // Return to shell
            current_pid = 0;
            shell_resume();
        }
    }
    
    // 7. Clean up PCB
    proc->state = PROC_STATE_EMPTY;
    
    return EOK;
}
```

### 7.2 Abnormal Termination

```c
// Called on fatal error (e.g., invalid memory access via syscall validation)
void proc_abort(int pid, const char* reason) {
    struct process* proc = proc_get(pid);
    
    term_puts("\n*** Process ");
    term_puts(proc->name);
    term_puts(" aborted: ");
    term_puts(reason);
    term_puts(" ***\n");
    
    proc_terminate(pid, -1);
}
```

---

## 8. Hardware Context Save (Recommended)

### 8.1 The Problem

Software context save/restore is error-prone:
- Must save all registers atomically
- Can't use registers while saving them
- Assembly code must be carefully crafted

### 8.2 How x86 (80486) Does It

The Intel 80486 has **hardware Task State Segment (TSS)** support:

- The CPU automatically saves registers when switching tasks
- A single instruction (`IRET` with different TSS) swaps contexts
- The TSS descriptor tells the CPU where to save/load registers

### 8.3 Recommended B32P3 Hardware Support

Add a minimal hardware context save mechanism:

**Option A: Shadow Register Set**
- Add a second set of 16 registers (shadow registers)
- New instruction `SWAPCTX` atomically swaps to shadow set
- Fast, but only supports 2 contexts

**Option B: Context Save to Memory**
- Add instruction `SAVECTX addr` - saves all registers to memory address
- Add instruction `LOADCTX addr` - restores all registers from memory
- Hardware ensures atomic save/restore
- Supports unlimited contexts (memory-limited)

**Option C: Context Stack (Simplest)**
- Add instruction `PUSHCTX` - pushes all registers to stack
- Add instruction `POPCTX` - pops all registers from stack
- Uses existing stack mechanism

### 8.4 Recommended Implementation

**We recommend Option B or C for B32P3:**

```
; Save context to memory (proposed instruction)
SAVECTX r1          ; Save r0-r15, PC to addresses [r1] through [r1+16]

; Restore context from memory (proposed instruction)
LOADCTX r1          ; Load r0-r15, PC from addresses [r1] through [r1+16]
                    ; Continues execution from loaded PC
```

This would simplify context switching to:

```c
void proc_switch(int from_pid, int to_pid) {
    struct process* from = proc_get(from_pid);
    struct process* to = proc_get(to_pid);
    
    // Hardware atomic save/restore
    asm(
        "loadaddr r1, from->saved_regs\n"
        "savectx r1\n"                     // Save current context
        "loadaddr r1, to->saved_regs\n"  
        "loadctx r1\n"                     // Restore and jump
    );
}
```

### 8.5 Without Hardware Support (Fallback)

If hardware support isn't added, use careful assembly:

```asm
; context_save: r4 = pointer to save area
; Must be called, not jumped to (r15 has return address)
context_save:
    write32 r1,  0, r4      ; Save r1
    write32 r2,  1, r4      ; Save r2
    write32 r3,  2, r4      ; Save r3
    ; ... r4 through r14 ...
    write32 r15, 15, r4     ; Save return address as PC
    jumpr 3, r15            ; Return

; context_restore: r4 = pointer to restore area
; Never returns to caller - jumps to saved PC
context_restore:
    read32 16, r4, r1       ; Load saved PC first
    push r1                 ; Save for later jump
    read32 1, r4, r1        ; Restore r1
    read32 2, r4, r2        ; Restore r2
    ; ... r3 through r14 ...
    read32 15, r4, r15      ; Restore r15
    read32 4, r4, r4        ; Finally restore r4
    pop r1                  ; Get saved PC
    jumpr 3, r1             ; Jump to saved PC
```

**Note**: This is tricky because we need r4 to address the save area, but we also need to save r4. The solution is to save r4 last (after we're done using it) and restore it last.

---

## 9. Scheduler Integration (Future)

For future versions that want timed task switching:

### 9.1 Simple Round-Robin (If Implemented)

```c
// kernel/sched/scheduler.c

#define TIME_SLICE_MS   100     // 100ms per process

static unsigned int slice_start;

void sched_tick(void) {
    unsigned int now = get_micros();
    
    if (current_pid > 0 && proc_ring_count > 1) {
        if ((now - slice_start) >= (TIME_SLICE_MS * 1000)) {
            // Time slice expired, switch to next
            switcher_next();
            slice_start = now;
        }
    }
}

// Called from timer interrupt
void timer_interrupt_handler(void) {
    sched_tick();
    bg_tick();
}
```

### 9.2 Why Not Enable by Default?

- **Complexity**: Full preemption requires saving/restoring at any point
- **Debugging**: Harder to reproduce bugs
- **User expectation**: Single-tasking is simpler for command-line OS

**Recommendation**: Keep manual switching for V2.0, add optional scheduler in V2.1.

---

## 10. Implementation Checklist

- [ ] Define `struct process` and process table
- [ ] Implement `proc_init()`
- [ ] Implement `context_save()` and `context_restore()` in assembly
- [ ] Implement `proc_create()` with argument passing
- [ ] Implement `proc_terminate()`
- [ ] Implement `proc_switch()`
- [ ] Implement process ring for switching
- [ ] Add hotkey detection in input subsystem
- [ ] Implement `switcher_next()`
- [ ] Add background task infrastructure
- [ ] Test switching between two simple programs
- [ ] Test program exit and cleanup

---

## 11. Summary

| Feature | Status | Notes |
|---------|--------|-------|
| Process states | Implemented | 6 states defined |
| Context switching | Hardware recommended | SAVECTX/LOADCTX instructions |
| Program slots | 14 slots | 512 KiW each, multi-slot support |
| Alt-Tab | Hotkey + ring | Ctrl+Tab recommended |
| Background tasks | Interrupt-driven | For keyboard, network |
| Scheduler | Future | Optional round-robin |
