# BDOS V2 Implementation Guide

**Prepared by: The Consultancy Team**  
**Marcus Rodriguez (Lead Systems Architect)**  
**Elena Vasquez (Software Architect)**  
**James O'Brien (File Systems & Storage Specialist)**  
**Sarah Chen (Embedded Systems Specialist)**  
**David Kim (User Experience & Interface Specialist)**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Updated for 64 MiB memory, PIC, software interrupt, raw Ethernet*

---

## 1. Introduction

This document provides a step-by-step implementation guide for BDOS V2. It synthesizes all previous reports into a concrete development plan with milestones, priorities, and practical code organization guidance.

---

## 2. Implementation Philosophy

### 2.1 Guiding Principles

1. **Incremental Development**: Build working systems at each stage
2. **Test Early**: Each component should be testable in isolation
3. **Reuse Existing Code**: Leverage existing libraries (BRFS, GPU, etc.)
4. **Keep It Simple**: Avoid over-engineering for a hobby project
5. **Document As You Go**: Maintain inline comments and header documentation

### 2.2 Development Approach

```
Phase 1: Foundation      (Weeks 1-2)
    ↓
Phase 2: Core Services   (Weeks 3-4)
    ↓
Phase 3: User Interface  (Weeks 5-6)
    ↓
Phase 4: Integration     (Weeks 7-8)
    ↓
Phase 5: Polish          (Week 9+)
```

---

## 3. Project Structure

### 3.1 Recommended Directory Layout

```
Software/C/BDOS/
├── kernel/
│   ├── main.c              # Kernel entry point
│   ├── init.c              # System initialization
│   ├── panic.c             # Kernel panic handler
│   │
│   ├── mem/
│   │   ├── mem.h           # Memory management header
│   │   ├── mem.c           # Memory allocator
│   │   └── mem_defs.h      # Memory layout constants
│   │
│   ├── proc/
│   │   ├── proc.h          # Process management header
│   │   ├── proc.c          # Process control
│   │   ├── sched.c         # Scheduler
│   │   └── switch.c        # Context switching
│   │
│   ├── fs/
│   │   ├── vfs.h           # Virtual filesystem header
│   │   ├── file.c          # File operations
│   │   ├── dir.c           # Directory operations
│   │   └── path.c          # Path utilities
│   │
│   ├── io/
│   │   ├── hid.h           # HID input header
│   │   ├── hid.c           # HID FIFO
│   │   ├── usb_keyboard.c  # USB keyboard driver
│   │   ├── term.h          # Terminal header
│   │   ├── term.c          # Terminal output
│   │   └── stream.c        # Stream abstraction
│   │
│   ├── net/
│   │   ├── net.h           # Network header
│   │   ├── enc28j60.c      # Ethernet driver
│   │   ├── bdos_proto.h    # BDOS raw Ethernet protocol
│   │   ├── bdos_proto.c    # Protocol handling
│   │   ├── netloader.c     # NetLoader service
│   │   └── nethid.c        # Network HID
│   │
│   ├── syscall/
│   │   ├── syscall.h       # Syscall definitions
│   │   ├── syscall.c       # Syscall dispatcher
│   │   ├── sys_file.c      # File syscalls
│   │   ├── sys_proc.c      # Process syscalls
│   │   ├── sys_io.c        # I/O syscalls
│   │   └── sys_mem.c       # Memory syscalls
│   │
│   └── shell/
│       ├── shell.h         # Shell header
│       ├── shell.c         # Shell main loop
│       ├── readline.c      # Line editing
│       ├── cmd_file.c      # File commands
│       ├── cmd_proc.c      # Process commands
│       ├── cmd_net.c       # Network commands
│       └── cmd_sys.c       # System commands
│
├── include/
│   ├── bdos.h              # Main BDOS header
│   ├── types.h             # Type definitions
│   ├── errno.h             # Error codes
│   └── config.h            # Build configuration
│
├── lib/                    # Kernel-internal libraries
│   ├── string.c            # String functions
│   ├── stdlib.c            # Standard library
│   └── printf.c            # Kernel printf
│
└── Makefile                # Build system
```

### 3.2 User Library Structure

```
Software/C/libs/user/
├── bdos.h                  # User program API
├── syscall.c               # Syscall wrappers
├── stdio.c                 # Standard I/O
├── stdlib.c                # Standard library
├── string.c                # String functions
└── crt0.asm                # C runtime startup
```

---

## 4. Phase 1: Foundation (Weeks 1-2)

### 4.1 Goals

- Boot to a working shell prompt
- Basic terminal I/O
- Memory layout established

### 4.2 Milestone 1.1: Minimal Boot

**Files to create:**
- `kernel/main.c`
- `kernel/init.c`
- `kernel/io/term.c`

```c
// kernel/main.c - Minimal starting point

// Entry point address for bootloader
#define BDOS_ENTRY 0x100000

// Include existing libraries
#include "libs/kernel/gpu_hal.h"
#include "libs/kernel/term.h"

void kernel_main(void) {
    // Initialize GPU
    gpu_init();
    
    // Initialize terminal
    term_init();
    
    // Print boot message
    term_puts("BDOS V2 Starting...\n");
    
    // Hang for now
    while (1) {
        // Will add shell here
    }
}

// Place entry at known address
void _start(void) __attribute__((section(".entry")));
void _start(void) {
    kernel_main();
}
```

**Test**: Boot and see message on screen.

### 4.3 Milestone 1.2: Keyboard Input

**Files to create:**
- `kernel/io/hid.c`
- `kernel/io/usb_keyboard.c`

```c
// kernel/io/hid.c - HID FIFO buffer

#define HID_FIFO_SIZE 256

static char hid_fifo[HID_FIFO_SIZE];
static unsigned int hid_head = 0;
static unsigned int hid_tail = 0;

void hid_push(char c) {
    unsigned int next = (hid_head + 1) % HID_FIFO_SIZE;
    if (next != hid_tail) {
        hid_fifo[hid_head] = c;
        hid_head = next;
    }
}

int hid_available(void) {
    return hid_head != hid_tail;
}

char hid_getchar(void) {
    while (!hid_available()) {
        usb_keyboard_poll();
    }
    char c = hid_fifo[hid_tail];
    hid_tail = (hid_tail + 1) % HID_FIFO_SIZE;
    return c;
}
```

**Test**: Type characters and see them echoed.

### 4.4 Milestone 1.3: Basic Shell Loop

**Files to create:**
- `kernel/shell/shell.c`
- `kernel/shell/readline.c`

```c
// kernel/shell/shell.c - Minimal shell

void shell_run(void) {
    char line[256];
    
    term_puts("BDOS V2 Shell\n");
    term_puts("Type 'help' for commands\n\n");
    
    while (1) {
        term_puts("> ");
        
        // Read line with basic editing
        int len = shell_readline(line, 256);
        if (len <= 0) continue;
        
        // Process command
        if (strcmp(line, "help") == 0) {
            term_puts("Available commands:\n");
            term_puts("  help  - Show this message\n");
            term_puts("  clear - Clear screen\n");
            term_puts("  echo  - Print text\n");
        }
        else if (strcmp(line, "clear") == 0) {
            term_clear();
        }
        else if (strncmp(line, "echo ", 5) == 0) {
            term_puts(&line[5]);
            term_puts("\n");
        }
        else {
            term_puts("Unknown command: ");
            term_puts(line);
            term_puts("\n");
        }
    }
}
```

**Test**: Interactive shell with basic commands.

---

## 5. Phase 2: Core Services (Weeks 3-4)

### 5.1 Goals

- File system mounted and working
- Memory allocator functional
- Basic process structure

### 5.2 Milestone 2.1: BRFS Integration

**Files to create:**
- `kernel/fs/vfs.h`
- `kernel/fs/file.c`

```c
// kernel/init.c - Add filesystem initialization

#include "libs/kernel/brfs.h"
#include "libs/kernel/spi_flash.h"

int init_filesystem(void) {
    term_puts("Initializing filesystem...\n");
    
    // Initialize SPI Flash
    spi_flash_init();
    
    // Mount BRFS
    int result = brfs_mount();
    if (result != BRFS_OK) {
        term_puts("  Failed to mount BRFS!\n");
        term_puts("  Creating new filesystem...\n");
        brfs_format("BDOS", 512, 65536);  // 32MB filesystem
        brfs_mount();
    }
    
    term_puts("  Filesystem mounted\n");
    return 0;
}
```

**Test**: `ls /` shows root directory contents.

### 5.3 Milestone 2.2: File Commands

**Files to add:**
- `kernel/shell/cmd_file.c`

Implement: `ls`, `cd`, `pwd`, `cat`, `mkdir`, `rm`

**Test**: Navigate filesystem, view files.

### 5.4 Milestone 2.3: Memory Allocator

**Files to create:**
- `kernel/mem/mem.c`

```c
// kernel/mem/mem.c - Simple allocator

#include "mem_defs.h"

#define HEAP_START  MEM_HEAP_START    // 0x0080000
#define HEAP_END    MEM_HEAP_END      // 0x0100000

struct block_header {
    unsigned int size;
    unsigned int used;
    struct block_header* next;
};

static struct block_header* free_list;
static int heap_initialized = 0;

void heap_init(void) {
    free_list = (struct block_header*)HEAP_START;
    free_list->size = HEAP_END - HEAP_START - sizeof(struct block_header);
    free_list->used = 0;
    free_list->next = NULL;
    heap_initialized = 1;
}

void* kmalloc(unsigned int size) {
    if (!heap_initialized) heap_init();
    
    // Find first fit
    struct block_header* block = free_list;
    while (block != NULL) {
        if (!block->used && block->size >= size) {
            // Split if too large
            if (block->size > size + sizeof(struct block_header) + 16) {
                struct block_header* new_block = 
                    (struct block_header*)((char*)block + sizeof(struct block_header) + size);
                new_block->size = block->size - size - sizeof(struct block_header);
                new_block->used = 0;
                new_block->next = block->next;
                
                block->size = size;
                block->next = new_block;
            }
            
            block->used = 1;
            return (void*)((char*)block + sizeof(struct block_header));
        }
        block = block->next;
    }
    
    return NULL;  // Out of memory
}

void kfree(void* ptr) {
    if (ptr == NULL) return;
    
    struct block_header* block = 
        (struct block_header*)((char*)ptr - sizeof(struct block_header));
    block->used = 0;
    
    // TODO: Coalesce with adjacent free blocks
}
```

**Test**: Allocate and free memory, verify no corruption.

### 5.5 Milestone 2.4: Process Structure

**Files to create:**
- `kernel/proc/proc.h`
- `kernel/proc/proc.c`

```c
// kernel/proc/proc.h

#define MAX_PROCESSES   14      // 14 × 512 KiW slots
#define PROC_NAME_LEN   16

enum proc_state {
    PROC_FREE = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
};

struct process {
    int pid;
    int first_slot;         // First slot (0-13)
    int slot_count;         // Number of slots used (1+ for large programs)
    char name[PROC_NAME_LEN];
    enum proc_state state;
    
    unsigned int code_start;
    unsigned int code_size;
    unsigned int stack_top;
    
    // Saved registers (for context switch)
    unsigned int saved_regs[16];
    unsigned int saved_pc;
    
    // I/O
    char cwd[64];
    
    // Flags
    unsigned int flags;
};

// Process table
extern struct process process_table[MAX_PROCESSES];
extern struct process* current_process;

// API
void proc_init(void);
struct process* proc_create(const char* name, unsigned int code_addr, int slot);
void proc_destroy(struct process* p);
struct process* proc_get_current(void);
```

**Test**: Create process structures, verify memory layout.

---

## 6. Phase 3: User Interface (Weeks 5-6)

### 6.1 Goals

- Load and run user programs
- Syscall interface working
- Alt-Tab process switching

### 6.2 Milestone 3.1: Program Loading

**Files to create:**
- `kernel/shell/exec.c`

```c
// kernel/shell/exec.c

int shell_run_program(const char* name) {
    // Find program file
    char path[128];
    sprintf(path, "/bin/%s", name);
    
    int fd = fs_open(path, O_READ);
    if (fd < 0) return -1;
    
    // Get size
    struct stat st;
    fs_stat(path, &st);
    
    // Find slot
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROC_FREE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        fs_close(fd);
        return -ENOMEM;
    }
    
    // Calculate load address
    unsigned int load_addr = PROG_SLOT_START + (slot * PROG_SLOT_SIZE);
    
    // Load program
    fs_read(fd, (void*)load_addr, st.st_size);
    fs_close(fd);
    
    // Create process
    struct process* p = proc_create(name, load_addr, slot);
    
    // Run it
    proc_start(p);
    
    return 0;
}
```

**Test**: Load and run simple program.

### 6.3 Milestone 3.2: Syscall Handler (Software Interrupt)

**Files to create:**
- `kernel/syscall/syscall.c`

**CPU Modification Required**: Add SOFTINT instruction to B32P3.

```c
// kernel/syscall/syscall.c
// Located at software interrupt vector (0x00000010)

void syscall_handler(void) {
    // Arguments in registers: r4=id, r5-r8=args
    int num = get_reg(4);
    int arg1 = get_reg(5);
    int arg2 = get_reg(6);
    int arg3 = get_reg(7);
    int arg4 = get_reg(8);
    
    int result = -ENOSYS;
    
    switch (num) {
        case SYS_EXIT:
            proc_exit(arg1);
            break;
            
        case SYS_READ:
            result = sys_read(arg1, (void*)arg2, arg3);
            break;
            
        case SYS_WRITE:
            result = sys_write(arg1, (void*)arg2, arg3);
            break;
            
        case SYS_OPEN:
            result = sys_open((char*)arg1, arg2);
            break;
            
        case SYS_CLOSE:
            result = sys_close(arg1);
            break;
            
        case SYS_PUTCHAR:
            term_putchar((char)arg1);
            result = arg1;
            break;
            
        case SYS_GETCHAR:
            result = hid_getchar();
            break;
            
        // Raw Ethernet syscalls
        case SYS_ETH_SEND:
            result = sys_eth_send((unsigned char*)arg1, arg2);
            break;
            
        case SYS_ETH_RECV:
            result = sys_eth_recv((unsigned char*)arg1, arg2);
            break;
            
        // ... more syscalls
    }
    
    // Return value in r4
    set_reg(4, result);
    
    // RETI returns to user code
}
```

**Test**: User program calls syscalls successfully.

### 6.4 Milestone 3.3: Process Switching

**Files to create:**
- `kernel/proc/switch.c`

```c
// kernel/proc/switch.c

void proc_switch_to(struct process* next) {
    struct process* current = proc_get_current();
    
    if (current == next) return;
    
    // Save current context
    if (current != NULL && current->state == PROC_RUNNING) {
        current->state = PROC_READY;
        // Save registers via inline assembly
        asm(
            "write32 r1, 0, %[regs]\n"
            "write32 r2, 1, %[regs]\n"
            // ... save all registers
            :
            : [regs] "r" (current->saved_regs)
        );
    }
    
    // Switch to next
    current_process = next;
    next->state = PROC_RUNNING;
    
    // Restore registers
    asm(
        "read32 1, %[regs], r1\n"
        "read32 2, %[regs], r2\n"
        // ... restore all registers
        "jumpr 0 r15\n"  // Return to saved PC
        :
        : [regs] "r" (next->saved_regs)
    );
}
```

**Test**: Alt-Tab switches between processes.

---

## 7. Phase 4: Integration (Weeks 7-8)

### 7.1 Goals

- Raw Ethernet networking functional
- NetLoader working (custom protocol, no IP)
- All components integrated

### 7.2 Milestone 4.1: Network Driver

**Files to create:**
- `kernel/net/enc28j60.c`
- `kernel/net/bdos_proto.h`

```c
// kernel/net/bdos_proto.h
// Custom BDOS raw Ethernet protocol (no IP/UDP/ARP)

#define ETH_TYPE_BDOS   0xBD05  // Custom EtherType

#define BDOS_PKT_DISCOVER   0x01
#define BDOS_PKT_ANNOUNCE   0x02
#define BDOS_PKT_DATA       0x10
#define BDOS_PKT_PROGRAM    0x20
#define BDOS_PKT_PROG_DATA  0x21
#define BDOS_PKT_PROG_END   0x22
#define BDOS_PKT_PROG_ACK   0x23
#define BDOS_PKT_HID        0x30

struct bdos_header {
    unsigned char version;
    unsigned char type;
    unsigned short seq;
    unsigned short length;
    unsigned short checksum;
};
```

**Test**: Device responds to DISCOVER packets from PC.

### 7.3 Milestone 4.2: NetLoader

**Files to create:**
- `kernel/net/netloader.c`

**Test**: Upload programs via raw Ethernet from PC Python tool.

### 7.4 Milestone 4.3: NetHID

**Files to create:**
- `kernel/io/nethid.c`

**Test**: Keyboard events from PC reach FPGC input queue.

---

## 8. Phase 5: Polish (Week 9+)

### 8.1 Goals

- Bug fixes and stability
- Documentation
- Example programs

### 8.2 Tasks

- [ ] Test all shell commands thoroughly
- [ ] Test edge cases (out of memory, full disk, etc.)
- [ ] Write example user programs
- [ ] Document syscall API
- [ ] Performance optimization if needed

---

## 9. Build System

### 9.1 Makefile

```makefile
# Software/C/BDOS/Makefile

CC = ../../../BuildTools/B32CC/smlrc
AS = python3 ../../../BuildTools/ASMPY/asmpy/asmpy.py

CFLAGS = -I include -I ../libs

# Source files
KERNEL_SRC = \
    kernel/main.c \
    kernel/init.c \
    kernel/panic.c \
    kernel/mem/mem.c \
    kernel/proc/proc.c \
    kernel/proc/sched.c \
    kernel/proc/switch.c \
    kernel/fs/file.c \
    kernel/fs/dir.c \
    kernel/fs/path.c \
    kernel/io/hid.c \
    kernel/io/usb_keyboard.c \
    kernel/io/term.c \
    kernel/io/stream.c \
    kernel/net/enc28j60.c \
    kernel/net/bdos_proto.c \
    kernel/net/netloader.c \
    kernel/net/nethid.c \
    kernel/syscall/syscall.c \
    kernel/shell/shell.c \
    kernel/shell/readline.c \
    kernel/shell/cmd_file.c \
    kernel/shell/cmd_proc.c \
    kernel/shell/cmd_net.c \
    kernel/shell/cmd_sys.c \
    lib/string.c \
    lib/stdlib.c \
    lib/printf.c

LIB_SRC = \
    ../libs/kernel/gpu_hal.c \
    ../libs/kernel/gpu_fb.c \
    ../libs/kernel/term.c \
    ../libs/kernel/brfs.c \
    ../libs/kernel/spi_flash.c \
    ../libs/kernel/malloc.c \
    ../libs/kernel/timer.c

# Combine all sources
ALL_SRC = $(KERNEL_SRC) $(LIB_SRC)

# Output files
ASM_OUTPUT = ../../ASM/Output/bdos.asm
BIN_OUTPUT = ../../ASM/Output/bdos.bin

.PHONY: all clean flash

all: $(BIN_OUTPUT)

# Compile C to ASM
$(ASM_OUTPUT): $(ALL_SRC)
	$(CC) $(CFLAGS) kernel/main.c -o $@

# Assemble to binary
$(BIN_OUTPUT): $(ASM_OUTPUT)
	$(AS) $< $@

# Flash to device
flash: $(BIN_OUTPUT)
	python3 ../../../Scripts/Programmer/UART/flash_uart.py $<

clean:
	rm -f $(ASM_OUTPUT) $(BIN_OUTPUT)
```

### 9.2 Build Commands

```bash
# Build BDOS
cd Software/C/BDOS
make

# Flash to device
make flash

# Clean build
make clean
```

---

## 10. Testing Strategy

### 10.1 Unit Testing

Where practical, test components in isolation:

```c
// Tests/C/bdos_tests/test_mem.c

void test_malloc(void) {
    // Test basic allocation
    void* p1 = kmalloc(100);
    assert(p1 != NULL);
    
    // Test allocation and free
    void* p2 = kmalloc(200);
    kfree(p1);
    void* p3 = kmalloc(50);
    
    // p3 should reuse p1's space
    assert(p3 == p1);
    
    kfree(p2);
    kfree(p3);
}
```

### 10.2 Integration Testing

Test full workflows:

1. Boot → Shell prompt
2. File operations (create, read, write, delete)
3. Program load and run
4. Process switching
5. Network upload
6. Background services

### 10.3 Stress Testing

- Fill filesystem
- Create maximum processes
- Rapid Alt-Tab switching
- Large file transfers

---

## 11. Debugging Tips

### 11.1 Debug Output

```c
// kernel/debug.h

#ifdef DEBUG
#define DPRINT(fmt, ...) kprintf("[DEBUG] " fmt, ##__VA_ARGS__)
#else
#define DPRINT(fmt, ...)
#endif
```

### 11.2 Panic Handler

```c
// kernel/panic.c

void kernel_panic(const char* msg) {
    // Disable interrupts
    asm("cli");
    
    term_puts("\n\n*** KERNEL PANIC ***\n");
    term_puts(msg);
    term_puts("\n");
    
    // Dump useful info
    term_puts("PC: ");
    term_puthex(get_pc());
    term_puts("\n");
    
    // Halt
    while (1);
}
```

### 11.3 Memory Dump

```c
void hexdump(void* addr, unsigned int len) {
    unsigned char* p = (unsigned char*)addr;
    for (unsigned int i = 0; i < len; i += 16) {
        term_puthex((unsigned int)(p + i));
        term_puts(": ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            term_puthex_byte(p[i + j]);
            term_putchar(' ');
        }
        term_puts("\n");
    }
}
```

---

## 12. Common Pitfalls

### 12.1 B32CC Limitations

Remember the compiler constraints:
- No struct return values
- Single-pass compilation
- Limited macro support
- No floating point

### 12.2 Memory Alignment

B32P3 is word-addressable:
```c
// Wrong - may cause issues
char buffer[100];  // Might not be word-aligned

// Better - ensure alignment
unsigned int buffer_words[25];  // 100 bytes, word-aligned
char* buffer = (char*)buffer_words;
```

### 12.3 Inline Assembly

Be careful with register usage:
```c
// Always list clobbered registers
asm(
    "load32 %[val], r1\n"
    "add r1, 1, r1\n"
    "write32 r1, 0, %[out]\n"
    : [out] "=m" (result)
    : [val] "r" (input)
    : "r1"  // IMPORTANT: Mark r1 as clobbered
);
```

---

## 13. Future Enhancements

After basic BDOS V2 is working:

1. **Shell Scripts**: Simple batch file execution
2. **Environment Variables**: PATH, HOME, etc.
3. **Signal Handling**: Full signal support
4. **Piping**: `cmd1 | cmd2` support
5. **TCP Support**: For HTTP server
6. **SD Card**: Additional storage
7. **Graphics Programs**: Beyond text mode

---

## 14. Summary

### Implementation Order

1. **Week 1**: Boot, terminal, keyboard
2. **Week 2**: Shell loop, basic commands
3. **Week 3**: BRFS mount, file commands
4. **Week 4**: Memory allocator, process structure
5. **Week 5**: Program loading, syscalls
6. **Week 6**: Process switching, Alt-Tab
7. **Week 7**: Network driver, IP/UDP
8. **Week 8**: NetLoader, integration
9. **Week 9+**: Polish, testing, documentation

### Key Files to Create First

1. `kernel/main.c` - Entry point
2. `kernel/io/term.c` - Terminal output
3. `kernel/io/hid.c` - Keyboard input
4. `kernel/shell/shell.c` - Command loop
5. `kernel/fs/file.c` - File operations
6. `kernel/proc/proc.c` - Process management
7. `kernel/syscall/syscall.c` - System calls

### Success Criteria

BDOS V2 is complete when:

- [x] Boots to interactive shell
- [x] Can navigate filesystem (`ls`, `cd`, `pwd`)
- [x] Can view files (`cat`)
- [x] Can manage files (`mkdir`, `rm`, `cp`)
- [x] Can load and run user programs
- [x] Can switch between programs (Alt-Tab)
- [x] Can upload programs via network
- [x] Can run background services
- [x] All shell commands documented
- [x] Example user programs working

---

*This concludes the BDOS V2 Implementation Guide. Good luck with your implementation!*

---

**Document Index**

1. [01-Executive-Summary.md](01-Executive-Summary.md) - Project overview and recommendations
2. [02-System-Architecture.md](02-System-Architecture.md) - Overall system design
3. [03-Memory-Management.md](03-Memory-Management.md) - Memory layout and allocation
4. [04-Process-Management.md](04-Process-Management.md) - Process model and scheduling
5. [05-System-Calls-IPC.md](05-System-Calls-IPC.md) - Syscall interface
6. [06-File-System-Integration.md](06-File-System-Integration.md) - BRFS integration
7. [07-Input-Subsystem.md](07-Input-Subsystem.md) - Keyboard and input handling
8. [08-Network-Subsystem.md](08-Network-Subsystem.md) - Network stack and services
9. [09-Shell-User-Programs.md](09-Shell-User-Programs.md) - Shell and user API
10. [10-Implementation-Guide.md](10-Implementation-Guide.md) - This document
