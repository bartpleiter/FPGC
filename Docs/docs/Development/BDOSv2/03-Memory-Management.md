# BDOS V2 Memory Management

**Prepared by: Marcus Rodriguez (Embedded Systems Specialist)**  
**Contributors: Dr. Emily Watson**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated for 64 MiB physical memory, position-independent code, 512 KiW slot granularity*

---

## 1. Overview

This document describes the memory management strategy for BDOS V2, including the memory map, heap allocation, program loading, and the position-independent code (PIC) model.

---

## 2. Hardware Memory Constraints

### 2.1 Physical Memory

| Region | Start Address | End Address | Size | Notes |
|--------|---------------|-------------|------|-------|
| SDRAM | 0x0000000 | 0x0FFFFFF | 16 MiW (64 MiB) | Physical device |
| I/O | 0x7000000 | 0x77FFFFF | 8 MiW | Memory-mapped I/O |
| ROM | 0x7800000 | 0x78003FF | 1 KiW | Bootloader |
| VRAM32 | 0x7900000 | 0x790041F | ~1 KiW | Pattern/Palette |
| VRAM8 | 0x7A00000 | 0x7A02001 | ~8 KiW | Tile tables |
| VRAMpx | 0x7B00000 | 0x7B12BFF | ~75 KiW | Pixel plane |

**Note**: The FPGA memory map supports up to 448 MiB, but the physical device has only 64 MiB. BDOS V2 targets this 64 MiB constraint.

### 2.2 Word Addressing

The B32P3 CPU uses **word addressing** (not byte addressing):
- Each address points to a 32-bit word (4 bytes)
- 1 MiW = 1 million words = 4 MiB
- 16 MiW = 64 MiB (our total available memory)

### 2.3 Stakeholder Requirement

From the requirements:
> "I want the OS to load the BRFS filesystem from one of the SPI Flash chips into the **last 32 MiB of RAM**"

32 MiB = 8 MiW, so BRFS occupies addresses 0x0800000 to 0x0FFFFFF.

---

## 3. BDOS V2 Memory Map

### 3.1 Detailed Memory Layout

```
Word Address   | Size      | Description
---------------+-----------+--------------------------------------------
0x0000000      |           | === KERNEL REGION ===
               | 512 KiW   | Kernel Code + Data (2 MiB)
0x0080000      |           |
---------------+-----------+--------------------------------------------
0x0080000      |           | === KERNEL HEAP ===
               | 512 KiW   | Dynamic allocation (2 MiB)
0x0100000      |           |
---------------+-----------+--------------------------------------------
0x0100000      |           | === USER PROGRAM REGION ===
               |           | 14 × 512 KiW slots (28 MiB)
               | 7 MiW     | Position-independent programs
               |           | Programs can span multiple slots
0x0800000      |           |
---------------+-----------+--------------------------------------------
0x0800000      |           | === BRFS CACHE ===
               | 8 MiW     | BRFS filesystem in RAM (32 MiB)
0x1000000      |           | (End of 64 MiB physical memory)
---------------+-----------+--------------------------------------------
```

### 3.2 Memory Map Defines

```c
// kernel/mem/mem_map.h

#ifndef MEM_MAP_H
#define MEM_MAP_H

// =============================================================================
// PHYSICAL MEMORY LIMITS (64 MiB device)
// =============================================================================
#define MEM_PHYSICAL_START      0x0000000
#define MEM_PHYSICAL_END        0x1000000   // 16 MiW = 64 MiB
#define MEM_PHYSICAL_SIZE       0x1000000

// =============================================================================
// KERNEL REGION (0x0000000 - 0x007FFFF) - 2 MiB
// =============================================================================
#define MEM_KERNEL_START        0x0000000
#define MEM_KERNEL_SIZE         0x0080000   // 512 KiW (2 MiB)
#define MEM_KERNEL_END          0x0080000

// Kernel stacks (at top of kernel region)
#define MEM_KERNEL_STACK_SIZE   0x0001000   // 4 KiW per stack
#define MEM_KERNEL_STACK_TOP    0x0080000   // Main stack grows down
#define MEM_KERNEL_INT_STACK_TOP 0x007F000  // Interrupt stack

// =============================================================================
// KERNEL HEAP (0x0080000 - 0x00FFFFF) - 2 MiB
// =============================================================================
#define MEM_HEAP_START          0x0080000
#define MEM_HEAP_SIZE           0x0080000   // 512 KiW (2 MiB)
#define MEM_HEAP_END            0x0100000

// =============================================================================
// USER PROGRAM REGION (0x0100000 - 0x07FFFFF) - 28 MiB
// =============================================================================
#define MEM_PROGRAM_START       0x0100000
#define MEM_PROGRAM_END         0x0800000
#define MEM_PROGRAM_SIZE        0x0700000   // 7 MiW (28 MiB)

// Slot configuration
#define MEM_SLOT_SIZE           0x0080000   // 512 KiW (2 MiB) per slot
#define MEM_SLOT_COUNT          14          // 7 MiW / 512 KiW = 14 slots

// Calculate slot base address
#define MEM_SLOT_BASE(n)        (MEM_PROGRAM_START + ((n) * MEM_SLOT_SIZE))

// =============================================================================
// BRFS CACHE (0x0800000 - 0x0FFFFFF) - 32 MiB
// =============================================================================
#define MEM_BRFS_START          0x0800000
#define MEM_BRFS_SIZE           0x0800000   // 8 MiW (32 MiB)
#define MEM_BRFS_END            0x1000000

// =============================================================================
// I/O MEMORY MAP
// =============================================================================
#define MEM_IO_START            0x7000000

#endif // MEM_MAP_H
```

---

## 4. Kernel Heap Allocator

### 4.1 Requirements

- Simple, deterministic allocation
- Minimal fragmentation
- No external dependencies
- Small code footprint

### 4.2 Design: First-Fit Free List

```c
// kernel/mem/heap.h

#ifndef HEAP_H
#define HEAP_H

#include "mem_map.h"

// Block header structure
struct heap_block {
    unsigned int size;          // Size in words (including header)
    unsigned int is_free;       // 0 = allocated, 1 = free
    struct heap_block* next;    // Next block
};

#define HEAP_BLOCK_HEADER_SIZE  3  // Size of heap_block in words

void heap_init(void);
void* kmalloc(unsigned int size);
void kfree(void* ptr);

#endif
```

### 4.3 Implementation

```c
// kernel/mem/heap.c

#include "heap.h"

static struct heap_block* heap_start;

void heap_init(void) {
    heap_start = (struct heap_block*)MEM_HEAP_START;
    heap_start->size = MEM_HEAP_SIZE;
    heap_start->is_free = 1;
    heap_start->next = 0;
}

void* kmalloc(unsigned int size) {
    if (size == 0) return 0;
    
    unsigned int total_size = size + HEAP_BLOCK_HEADER_SIZE;
    struct heap_block* current = heap_start;
    
    while (current != 0) {
        if (current->is_free && current->size >= total_size) {
            // Split if remaining space is large enough
            if (current->size > total_size + HEAP_BLOCK_HEADER_SIZE + 4) {
                struct heap_block* new_block = 
                    (struct heap_block*)((unsigned int*)current + total_size);
                new_block->size = current->size - total_size;
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = total_size;
                current->next = new_block;
            }
            
            current->is_free = 0;
            return (void*)((unsigned int*)current + HEAP_BLOCK_HEADER_SIZE);
        }
        current = current->next;
    }
    
    return 0;  // Out of memory
}

void kfree(void* ptr) {
    if (ptr == 0) return;
    
    struct heap_block* block = 
        (struct heap_block*)((unsigned int*)ptr - HEAP_BLOCK_HEADER_SIZE);
    
    block->is_free = 1;
    
    // Coalesce with next block if free
    if (block->next != 0 && block->next->is_free) {
        block->size = block->size + block->next->size;
        block->next = block->next->next;
    }
}
```

---

## 5. Position-Independent Code (PIC)

### 5.1 Rationale

Compiling programs for fixed slot addresses requires:
- Compiling each program multiple times for different slots, OR
- Complex runtime relocation

Instead, **position-independent code** allows programs to run at any address:
- Programs are compiled once
- Loaded into any available slot(s) at runtime
- Large programs can span multiple consecutive slots

### 5.2 Implementation in Assembler (ASMPY)

The key insight: **B32P3 already has relative jump instructions!**

| Instruction | Type | Description |
|-------------|------|-------------|
| `JUMP addr` | Absolute | Jump to fixed address |
| `JUMPO offset` | Relative | Jump to PC + offset |
| `JUMPR cond reg` | Register | Jump to address in register |

**PIC Assembly Strategy:**

1. **During assembly, convert absolute JUMPs to relative JUMPOs**
2. **Data references use PC-relative calculation**

**ASMPY Modification (Post-Processing Pass):**

```python
# In ASMPY assembler - add as final pass

def convert_to_pic(instructions, labels, current_addresses):
    """
    Convert absolute jumps to relative jumps for PIC.
    Run after all labels are resolved but before binary output.
    """
    pic_instructions = []
    
    for addr, instr in enumerate(instructions):
        opcode = get_opcode(instr)
        
        if opcode == OP_JUMP:
            # Extract target address from instruction
            target = get_jump_target(instr)
            
            # Calculate relative offset
            offset = target - addr
            
            # Create JUMPO instruction with offset
            new_instr = make_jumpo(offset)
            pic_instructions.append(new_instr)
            
        elif is_load_label(instr):
            # For addr2reg pseudo-instruction referencing a label
            # Convert to PC-relative: savpc + add
            target = get_label_address(instr)
            dest_reg = get_dest_reg(instr)
            offset = target - addr
            
            # Replace with: savpc dest; add dest, offset, dest
            pic_instructions.append(make_savpc(dest_reg))
            pic_instructions.append(make_add_imm(dest_reg, offset, dest_reg))
            
        else:
            # Keep instruction unchanged
            pic_instructions.append(instr)
    
    return pic_instructions
```

### 5.3 Data Address References

For accessing global data, use PC-relative addressing:

**Original (absolute):**
```asm
load32 my_data, r1     ; Load absolute address of my_data into r1
read32 0, r1, r2       ; Read from my_data
```

**PIC version:**
```asm
savpc r1               ; r1 = current PC
add r1, @offset_to_my_data, r1  ; r1 = PC + offset to my_data
read32 0, r1, r2       ; Read from my_data
```

### 5.4 Function Calls in PIC

**Original:**
```asm
jump my_function       ; Absolute jump
```

**PIC version:**
```asm
jumpo @offset_to_function  ; Relative jump (PC + offset)
```

### 5.5 ASMPY PIC Mode Flag

Add a flag to enable PIC mode:

```bash
# Assemble as position-independent code
python3 asmpy.py --pic program.asm -o program.bin
```

When `--pic` is enabled:
1. All `JUMP label` → `JUMPO offset`
2. All `addr2reg label, reg` → `SAVPC + ADD` sequence
3. Final binary contains only relative references

### 5.6 Example Transformation

**Input assembly:**
```asm
.code
Main:
    addr2reg message, r1   ; Get address of message
    jump PrintChar         ; Call function
    halt

PrintChar:
    read32 0, r1, r2       ; Read character
    write32 r2, 0, r3      ; Write to output (r3 = output addr)
    jumpr 3 r15            ; Return

.data
message:
    .dw 0x48656C6C         ; "Hell"
    .dw 0x6F000000         ; "o\0\0\0"
```

**PIC output (conceptual):**
```asm
; At address 0 (wherever loaded)
Main:
    savpc r1               ; r1 = PC of this instruction
    add r1, 7, r1          ; r1 = PC + offset to message (message is 7 words away)
    jumpo 2                ; Jump forward 2 words to PrintChar
    halt

PrintChar:
    read32 0, r1, r2       ; Read character
    write32 r2, 0, r3      ; Write to output
    jumpr 3 r15            ; Return (already register-relative)

message:
    .dw 0x48656C6C
    .dw 0x6F000000
```

### 5.7 Implementation Complexity Assessment

The PIC implementation in ASMPY is **straightforward** because:

1. **JUMPO already exists**: No new instructions needed in hardware
2. **SAVPC already exists**: Can get current PC easily
3. **Single-pass feasible**: Labels resolved before PIC conversion
4. **Post-processing pass**: Clean separation from main assembly

**Estimated implementation effort**: 50-100 lines of Python in ASMPY.

---

## 6. Program Slot Management

### 6.1 Slot Architecture with PIC

With position-independent code, slots are uniform:

| Slot | Base Address | Size |
|------|--------------|------|
| 0 | 0x0100000 | 512 KiW (2 MiB) |
| 1 | 0x0180000 | 512 KiW (2 MiB) |
| 2 | 0x0200000 | 512 KiW (2 MiB) |
| ... | ... | ... |
| 13 | 0x0780000 | 512 KiW (2 MiB) |

### 6.2 Multi-Slot Programs

Large programs (like Doom) can span multiple consecutive slots:

```c
// kernel/proc/slot.h

#define SLOT_FREE           0
#define SLOT_START          1   // First slot of a program
#define SLOT_CONTINUATION   2   // Additional slot for large program

struct slot_info {
    unsigned int status;        // SLOT_FREE, SLOT_START, SLOT_CONTINUATION
    unsigned int program_id;    // ID of program using this slot
    unsigned int slot_count;    // Number of slots used (only for SLOT_START)
};

extern struct slot_info slots[MEM_SLOT_COUNT];
```

### 6.3 Slot Allocation

```c
// kernel/proc/slot.c

// Find N consecutive free slots
// Returns first slot index, or -1 if not found
int slot_find_consecutive(int count) {
    int consecutive = 0;
    int start = -1;
    int i;
    
    for (i = 0; i < MEM_SLOT_COUNT; i++) {
        if (slots[i].status == SLOT_FREE) {
            if (consecutive == 0) {
                start = i;
            }
            consecutive++;
            if (consecutive >= count) {
                return start;
            }
        } else {
            consecutive = 0;
            start = -1;
        }
    }
    
    return -1;  // Not enough consecutive slots
}

// Allocate slots for a program
int slot_allocate(int slot_count, int program_id) {
    int first = slot_find_consecutive(slot_count);
    int i;
    
    if (first < 0) return -1;
    
    // Mark first slot
    slots[first].status = SLOT_START;
    slots[first].program_id = program_id;
    slots[first].slot_count = slot_count;
    
    // Mark continuation slots
    for (i = 1; i < slot_count; i++) {
        slots[first + i].status = SLOT_CONTINUATION;
        slots[first + i].program_id = program_id;
        slots[first + i].slot_count = 0;
    }
    
    return first;
}

// Free slots used by a program
void slot_free(int first_slot) {
    int count;
    int i;
    
    if (slots[first_slot].status != SLOT_START) return;
    
    count = slots[first_slot].slot_count;
    for (i = 0; i < count; i++) {
        slots[first_slot + i].status = SLOT_FREE;
        slots[first_slot + i].program_id = 0;
        slots[first_slot + i].slot_count = 0;
    }
}

// Get base address for slot
unsigned int slot_get_base(int slot) {
    return MEM_SLOT_BASE(slot);
}
```

---

## 7. Program Binary Format

### 7.1 Program Header

Programs include a simple header for metadata:

```c
// include/program.h

struct program_header {
    unsigned int magic;         // 0x42444F53 ("BDOS")
    unsigned int version;       // Header version (1)
    unsigned int code_size;     // Total size in words (including header)
    unsigned int entry_offset;  // Entry point offset from load address
    unsigned int stack_size;    // Requested stack size in words
    unsigned int flags;         // Program flags
    unsigned int min_slots;     // Minimum slots required
    unsigned int reserved;      // Reserved for future use
};

#define PROGRAM_MAGIC           0x42444F53
#define PROGRAM_HEADER_SIZE     8   // 8 words

// Program flags
#define PROG_FLAG_PIC           0x01    // Position-independent (always set)
#define PROG_FLAG_NEEDS_NET     0x02    // Requires network access
#define PROG_FLAG_BACKGROUND    0x04    // Can run as background task
```

### 7.2 Program Layout in Memory

```
+-------------------+ <- Slot base
|  Program Header   |    8 words
+-------------------+
|  Code + Data      |    Position-independent
|  (combined)       |    (assembler moves .data after .code)
+-------------------+
|       ...         |    Free space
+-------------------+
|  Stack            |    Grows downward
|       ↓           |
+-------------------+ <- Slot base + (slots × SLOT_SIZE)
```

---

## 8. Program Loading

### 8.1 Load Process

```c
// kernel/proc/loader.c

int load_program(const char* path) {
    int fd;
    struct program_header header;
    int slots_needed;
    int first_slot;
    unsigned int* load_addr;
    
    // 1. Open file
    fd = fs_open(path, O_READ);
    if (fd < 0) return -1;
    
    // 2. Read header
    fs_read(fd, &header, PROGRAM_HEADER_SIZE);
    
    // 3. Validate
    if (header.magic != PROGRAM_MAGIC) {
        fs_close(fd);
        return -2;
    }
    
    // 4. Calculate slots needed
    slots_needed = (header.code_size + MEM_SLOT_SIZE - 1) / MEM_SLOT_SIZE;
    if (slots_needed < header.min_slots) {
        slots_needed = header.min_slots;
    }
    
    // 5. Allocate slots
    first_slot = slot_allocate(slots_needed, next_program_id());
    if (first_slot < 0) {
        fs_close(fd);
        return -3;  // No space
    }
    
    // 6. Load to slot base address
    load_addr = (unsigned int*)slot_get_base(first_slot);
    
    // Rewind to start of file
    fs_seek(fd, 0, SEEK_SET);
    
    // Read entire program
    fs_read(fd, load_addr, header.code_size);
    fs_close(fd);
    
    // 7. Create process
    return proc_create(path, first_slot, slots_needed);
}
```

---

## 9. Stack Management

### 9.1 Stack Per Program

Each program's stack is at the top of its allocated memory:

```c
unsigned int* init_program_stack(int first_slot, int slot_count) {
    unsigned int* slot_end;
    
    // Stack is at the very top of the program's memory
    slot_end = (unsigned int*)(
        slot_get_base(first_slot) + (slot_count * MEM_SLOT_SIZE)
    );
    
    // Return stack pointer (stack grows down)
    return slot_end - 1;
}
```

---

## 10. Memory Protection (Software-Based)

### 10.1 Syscall Argument Validation

Since there's no MMU, validate memory addresses in syscalls:

```c
// kernel/syscall/validate.c

int validate_user_address(unsigned int addr, unsigned int size, int proc_id) {
    struct process* p;
    unsigned int slot_base;
    unsigned int slot_end;
    
    p = proc_get(proc_id);
    if (p == 0) return -1;
    
    slot_base = slot_get_base(p->first_slot);
    slot_end = slot_base + (p->slot_count * MEM_SLOT_SIZE);
    
    // Check bounds
    if (addr < slot_base) return -1;
    if (addr + size > slot_end) return -1;
    
    return 0;  // Valid
}
```

---

## 11. Implementation Checklist

- [ ] Create `kernel/mem/mem_map.h` with all addresses
- [ ] Implement `heap_init()`, `kmalloc()`, `kfree()`
- [ ] Add `--pic` flag to ASMPY assembler
- [ ] Implement JUMP → JUMPO conversion in ASMPY
- [ ] Implement addr2reg label → SAVPC+ADD conversion
- [ ] Implement slot allocation (single and multi-slot)
- [ ] Implement program header parsing
- [ ] Implement `load_program()` from filesystem
- [ ] Implement `validate_user_address()`
- [ ] Test PIC programs at different slot addresses
- [ ] Test multi-slot program loading

---

## 12. Summary

| Component | Size | Address Range |
|-----------|------|---------------|
| Kernel | 512 KiW (2 MiB) | 0x0000000 - 0x007FFFF |
| Kernel Heap | 512 KiW (2 MiB) | 0x0080000 - 0x00FFFFF |
| Program Slots (14×) | 7 MiW (28 MiB) | 0x0100000 - 0x07FFFFF |
| BRFS Cache | 8 MiW (32 MiB) | 0x0800000 - 0x0FFFFFF |
| **Total** | 16 MiW (64 MiB) | |
