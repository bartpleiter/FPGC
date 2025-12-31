# BDOS V2 Shell & User Programs

**Prepared by: Elena Vasquez (Software Architect)**  
**Contributors: Marcus Rodriguez, James O'Brien**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Shell in kernel confirmed, PIC programs, 14 slots*

---

## 1. Overview

This document describes the shell design and user program interface for BDOS V2, including program loading, execution model, and the user-space API.

---

## 2. Shell Design

### 2.1 Shell in Kernel

The shell runs as part of the kernel (confirmed approach):

```
┌─────────────────────────────────────────┐
│                Kernel                    │
│  ┌───────────────────────────────────┐  │
│  │            Shell Code             │  │
│  │  (command parsing, built-ins)     │  │
│  └───────────────────────────────────┘  │
│                   │                      │
│                   ▼                      │
│        User Programs (14 slots)          │
└─────────────────────────────────────────┘
```

**Rationale:**
- Simple implementation
- Direct access to kernel functions
- No context switch for built-in commands
- Shell code is small, fits easily in kernel space

---

## 3. Shell Architecture

### 3.1 Shell Components

```c
// kernel/shell/shell.h

#ifndef SHELL_H
#define SHELL_H

// Shell entry point
void shell_init(void);
void shell_run(void);

// Command handling
void shell_process_line(char* line);
int shell_parse_args(char* line, char** argv, int max_args);

// Built-in commands
typedef void (*shell_cmd_fn)(int argc, char** argv);

struct shell_command {
    const char* name;
    shell_cmd_fn func;
    const char* help;
};

// Register commands
void shell_register_command(const char* name, shell_cmd_fn fn, 
                           const char* help);

// Program execution
int shell_run_program(const char* name);

#endif
```

### 3.2 Shell Main Loop

```c
// kernel/shell/shell.c

#include "shell.h"
#include "readline.h"
#include "term.h"

#define MAX_ARGS    16
#define MAX_LINE    256

static char prompt[32] = "BDOS> ";
static int shell_running = 1;

void shell_init(void) {
    // Register built-in commands
    shell_register_command("help", cmd_help, "Show available commands");
    shell_register_command("ls", cmd_ls, "List directory contents");
    shell_register_command("cd", cmd_cd, "Change directory");
    shell_register_command("pwd", cmd_pwd, "Print working directory");
    shell_register_command("cat", cmd_cat, "Display file contents");
    shell_register_command("mkdir", cmd_mkdir, "Create directory");
    shell_register_command("rm", cmd_rm, "Remove file");
    shell_register_command("cp", cmd_cp, "Copy file");
    shell_register_command("mv", cmd_mv, "Move/rename file");
    shell_register_command("clear", cmd_clear, "Clear screen");
    shell_register_command("echo", cmd_echo, "Print text");
    shell_register_command("sync", cmd_sync, "Sync filesystem to flash");
    shell_register_command("df", cmd_df, "Show filesystem usage");
    shell_register_command("ifconfig", cmd_ifconfig, "Network configuration");
    shell_register_command("ping", cmd_ping, "Ping host");
    shell_register_command("ps", cmd_ps, "List processes");
    shell_register_command("kill", cmd_kill, "Terminate process");
    shell_register_command("reboot", cmd_reboot, "Reboot system");
    shell_register_command("exit", cmd_exit, "Exit shell");
}

void shell_run(void) {
    char line[MAX_LINE];
    char* argv[MAX_ARGS];
    
    term_puts("\nBDOS V2 Shell\n");
    term_puts("Type 'help' for available commands\n\n");
    
    while (shell_running) {
        // Show prompt with current directory
        char cwd[64];
        fs_getcwd(cwd, sizeof(cwd));
        term_puts(cwd);
        term_puts("> ");
        
        // Read input
        int len = readline(line, MAX_LINE, NULL);
        
        if (len < 0) {
            // Interrupted (Ctrl+C)
            term_puts("\n");
            continue;
        }
        
        if (len == 0) continue;
        
        // Process command
        shell_process_line(line);
    }
}
```

### 3.3 Command Parsing

```c
// kernel/shell/shell.c (continued)

// Maximum registered commands
#define MAX_COMMANDS    32

static struct shell_command commands[MAX_COMMANDS];
static int num_commands = 0;

void shell_register_command(const char* name, shell_cmd_fn fn,
                           const char* help) {
    if (num_commands < MAX_COMMANDS) {
        commands[num_commands].name = name;
        commands[num_commands].func = fn;
        commands[num_commands].help = help;
        num_commands++;
    }
}

int shell_parse_args(char* line, char** argv, int max_args) {
    int argc = 0;
    int in_arg = 0;
    int in_quote = 0;
    
    for (int i = 0; line[i] != '\0' && argc < max_args; i++) {
        char c = line[i];
        
        if (c == '"') {
            in_quote = !in_quote;
            if (!in_arg) {
                argv[argc++] = &line[i + 1];
                in_arg = 1;
            }
        } else if ((c == ' ' || c == '\t') && !in_quote) {
            if (in_arg) {
                line[i] = '\0';
                in_arg = 0;
            }
        } else {
            if (!in_arg) {
                argv[argc++] = &line[i];
                in_arg = 1;
            }
        }
    }
    
    return argc;
}

void shell_process_line(char* line) {
    char* argv[MAX_ARGS];
    int argc = shell_parse_args(line, argv, MAX_ARGS);
    
    if (argc == 0) return;
    
    // Check built-in commands
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }
    
    // Try to run as external program
    int result = shell_run_program(argv[0]);
    if (result < 0) {
        term_puts(argv[0]);
        term_puts(": command not found\n");
    }
}
```

---

## 4. Built-in Commands

### 4.1 Help Command

```c
// kernel/shell/cmd_misc.c

void cmd_help(int argc, char** argv) {
    if (argc > 1) {
        // Show help for specific command
        for (int i = 0; i < num_commands; i++) {
            if (strcmp(argv[1], commands[i].name) == 0) {
                term_puts(commands[i].name);
                term_puts(" - ");
                term_puts(commands[i].help);
                term_puts("\n");
                return;
            }
        }
        term_puts("Unknown command: ");
        term_puts(argv[1]);
        term_puts("\n");
    } else {
        // List all commands
        term_puts("Available commands:\n");
        for (int i = 0; i < num_commands; i++) {
            term_puts("  ");
            term_puts(commands[i].name);
            
            // Pad to column 12
            int len = strlen(commands[i].name);
            for (int j = len; j < 12; j++) {
                term_putchar(' ');
            }
            
            term_puts(commands[i].help);
            term_puts("\n");
        }
        term_puts("\nRun a program by typing its name.\n");
    }
}
```

### 4.2 Process Commands

```c
// kernel/shell/cmd_proc.c

void cmd_ps(int argc, char** argv) {
    term_puts("PID  STATE      NAME\n");
    term_puts("---  -----      ----\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct process* p = proc_get(i);
        if (p == NULL || p->state == PROC_FREE) continue;
        
        // PID
        term_putint(p->pid);
        term_puts("    ");
        
        // State
        switch (p->state) {
            case PROC_RUNNING:  term_puts("RUNNING    "); break;
            case PROC_READY:    term_puts("READY      "); break;
            case PROC_BLOCKED:  term_puts("BLOCKED    "); break;
            case PROC_ZOMBIE:   term_puts("ZOMBIE     "); break;
            default:            term_puts("UNKNOWN    "); break;
        }
        
        // Name
        term_puts(p->name);
        
        // Mark foreground
        if (p == proc_get_foreground()) {
            term_puts(" [fg]");
        }
        
        term_puts("\n");
    }
}

void cmd_kill(int argc, char** argv) {
    if (argc < 2) {
        term_puts("Usage: kill <pid>\n");
        return;
    }
    
    int pid = atoi(argv[1]);
    struct process* p = proc_get_by_pid(pid);
    
    if (p == NULL) {
        term_puts("No such process\n");
        return;
    }
    
    if (p == proc_get_current()) {
        term_puts("Cannot kill current process\n");
        return;
    }
    
    proc_kill(p);
    term_puts("Killed process ");
    term_putint(pid);
    term_puts("\n");
}
```

### 4.3 System Commands

```c
// kernel/shell/cmd_sys.c

void cmd_clear(int argc, char** argv) {
    term_clear();
}

void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) term_putchar(' ');
        term_puts(argv[i]);
    }
    term_puts("\n");
}

void cmd_reboot(int argc, char** argv) {
    term_puts("Syncing filesystem...\n");
    fs_sync();
    
    term_puts("Rebooting...\n");
    
    // Jump to reset vector
    asm("jump 0x7800000\n");  // ROM start
}

void cmd_exit(int argc, char** argv) {
    // If shell is in kernel, this does nothing meaningful
    // If shell is a user program, this would exit
    term_puts("Shell cannot exit (it's the kernel!)\n");
}
```

---

## 5. User Program Loading

### 5.1 Program Location

Programs are stored in `/bin/` directory:

```
/bin/
  hello
  editor
  forth
  mandelbrot
  ...
```

### 5.2 Program Format

Programs are simple binary blobs:
- **No headers**: Just raw machine code
- **Fixed load address**: 0x400000 for first slot
- **Entry point**: First instruction (address 0)

### 5.3 Loading Process

```c
// kernel/shell/exec.c

#include "proc.h"
#include "fs.h"

#define PROG_SLOT_SIZE  0x200000  // 2 MiW per program

int shell_run_program(const char* name) {
    char path[128];
    
    // Try current directory first
    fs_getcwd(path, sizeof(path));
    strcat(path, "/");
    strcat(path, name);
    
    int fd = fs_open(path, O_READ);
    
    if (fd < 0) {
        // Try /bin/
        strcpy(path, "/bin/");
        strcat(path, name);
        fd = fs_open(path, O_READ);
    }
    
    if (fd < 0) {
        return -ENOENT;
    }
    
    // Get file size
    struct stat st;
    fs_stat(path, &st);
    unsigned int size = st.st_size;
    
    // Find free process slot
    int slot = proc_find_free_slot();
    if (slot < 0) {
        fs_close(fd);
        return -ENOMEM;
    }
    
    // Calculate load address
    unsigned int load_addr = PROG_SLOT_START + (slot * PROG_SLOT_SIZE);
    
    // Load program into memory
    term_puts("Loading ");
    term_puts(name);
    term_puts("...\n");
    
    fs_read(fd, (void*)load_addr, size);
    fs_close(fd);
    
    // Create process
    struct process* p = proc_create(name, load_addr, slot);
    if (p == NULL) {
        return -ENOMEM;
    }
    
    // Add to process ring
    proc_ring_add(p);
    
    // Make it the foreground process
    proc_set_foreground(p);
    
    // Start execution
    proc_start(p);
    
    return 0;
}
```

### 5.4 Process Startup

```c
// kernel/proc/start.c

void proc_start(struct process* p) {
    // Save current process state
    struct process* current = proc_get_current();
    if (current != NULL && current != p) {
        proc_save_state(current);
    }
    
    // Switch to new process
    proc_set_current(p);
    p->state = PROC_RUNNING;
    
    // Initialize process registers
    // r0 = 0 (always)
    // r1 = argc
    // r2 = argv pointer (if we support args)
    // r15 = return address (to process exit handler)
    
    // Jump to entry point
    unsigned int entry = p->code_start;
    unsigned int exit_handler = (unsigned int)&proc_exit_handler;
    
    asm(
        "load32 %[entry], r14\n"      // Load entry point
        "load32 %[exit], r15\n"        // Load exit handler address
        "jumpr 0 r14\n"                // Jump to program
        :
        : [entry] "r" (entry),
          [exit] "r" (exit_handler)
        : "r14", "r15"
    );
}

// Called when process returns or calls exit
void proc_exit_handler(void) {
    struct process* p = proc_get_current();
    int exit_code = 0;  // Could get from r1
    
    // Clean up process
    proc_cleanup(p);
    
    // Remove from ring
    proc_ring_remove(p);
    
    // Switch to next process or shell
    struct process* next = proc_ring_next(NULL);
    if (next != NULL) {
        proc_switch_to(next);
    } else {
        // Return to shell
        shell_run();
    }
}
```

---

## 6. User Program API

### 6.1 User Library Structure

Programs link against a user library providing syscall wrappers:

```c
// libs/user/bdos.h

#ifndef BDOS_H
#define BDOS_H

// === Process control ===
void exit(int status);
int getpid(void);

// === File I/O ===
int open(const char* path, int flags);
int close(int fd);
int read(int fd, void* buf, unsigned int count);
int write(int fd, const void* buf, unsigned int count);
int seek(int fd, int offset, int whence);

// === Console I/O ===
int putchar(int c);
int puts(const char* s);
int getchar(void);
int printf(const char* fmt, ...);
char* gets(char* buf);

// === File system ===
int mkdir(const char* path);
int rmdir(const char* path);
int unlink(const char* path);
int chdir(const char* path);
char* getcwd(char* buf, unsigned int size);

// === Memory ===
void* malloc(unsigned int size);
void free(void* ptr);
void* realloc(void* ptr, unsigned int size);

// === Time ===
unsigned int time(void);
void sleep(unsigned int ms);

// === System ===
int system(const char* cmd);

#endif
```

### 6.2 Syscall Wrapper Implementation

```c
// libs/user/syscall.c

#define SYS_ENTRY   0x08C0001   // Syscall entry point

// Generic syscall with up to 4 arguments
static int syscall(int num, int a1, int a2, int a3, int a4) {
    int result;
    
    // Set up syscall frame at 0x08C0000
    volatile unsigned int* frame = (volatile unsigned int*)0x08C0000;
    frame[0] = num;     // Syscall number
    frame[1] = a1;      // Arg 1
    frame[2] = a2;      // Arg 2
    frame[3] = a3;      // Arg 3
    frame[4] = a4;      // Arg 4
    
    // Call kernel
    asm(
        "savpc r15\n"           // Save return address
        "jump @SYS_ENTRY\n"     // Jump to kernel
        :
        :
        : "r15"
    );
    
    // Get result from frame
    result = frame[5];
    
    return result;
}

// === Wrapper functions ===

void exit(int status) {
    syscall(SYS_EXIT, status, 0, 0, 0);
    // Never returns
    while(1);
}

int open(const char* path, int flags) {
    return syscall(SYS_OPEN, (int)path, flags, 0, 0);
}

int close(int fd) {
    return syscall(SYS_CLOSE, fd, 0, 0, 0);
}

int read(int fd, void* buf, unsigned int count) {
    return syscall(SYS_READ, fd, (int)buf, count, 0);
}

int write(int fd, const void* buf, unsigned int count) {
    return syscall(SYS_WRITE, fd, (int)buf, count, 0);
}

int putchar(int c) {
    return syscall(SYS_PUTCHAR, c, 0, 0, 0);
}

int getchar(void) {
    return syscall(SYS_GETCHAR, 0, 0, 0, 0);
}

void* malloc(unsigned int size) {
    return (void*)syscall(SYS_MALLOC, size, 0, 0, 0);
}

void free(void* ptr) {
    syscall(SYS_FREE, (int)ptr, 0, 0, 0);
}

unsigned int time(void) {
    return syscall(SYS_TIME, 0, 0, 0, 0);
}

void sleep(unsigned int ms) {
    syscall(SYS_SLEEP, ms, 0, 0, 0);
}
```

### 6.3 printf Implementation

```c
// libs/user/printf.c

#include "bdos.h"

int printf(const char* fmt, ...) {
    // Simple printf implementation
    // (Could also be a syscall to kernel printf)
    
    char buf[256];
    int len = 0;
    
    va_list args;
    va_start(args, fmt);
    
    while (*fmt && len < 255) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':
                case 'i': {
                    int val = va_arg(args, int);
                    len += int_to_str(val, &buf[len], 10);
                    break;
                }
                case 'x': {
                    int val = va_arg(args, int);
                    len += int_to_str(val, &buf[len], 16);
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    while (*s && len < 255) {
                        buf[len++] = *s++;
                    }
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    buf[len++] = c;
                    break;
                }
                case '%':
                    buf[len++] = '%';
                    break;
            }
        } else {
            buf[len++] = *fmt;
        }
        fmt++;
    }
    
    va_end(args);
    
    buf[len] = '\0';
    
    // Output via syscall
    return write(1, buf, len);  // fd 1 = stdout
}
```

---

## 7. Example User Program

### 7.1 Hello World

```c
// Software/C/userBDOS/hello.c

#include "bdos.h"

int main(void) {
    printf("Hello from BDOS V2!\n");
    printf("My PID is: %d\n", getpid());
    
    printf("Press any key to exit...\n");
    getchar();
    
    return 0;
}
```

### 7.2 File Listing Program

```c
// Software/C/userBDOS/dirlist.c

#include "bdos.h"

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : ".";
    
    int fd = opendir(path);
    if (fd < 0) {
        printf("Cannot open directory: %s\n", path);
        return 1;
    }
    
    struct dirent entry;
    printf("Contents of %s:\n", path);
    
    while (readdir(fd, &entry) > 0) {
        if (entry.type == FT_DIR) {
            printf("[DIR]  %s\n", entry.name);
        } else {
            printf("       %s (%d bytes)\n", entry.name, entry.size);
        }
    }
    
    closedir(fd);
    return 0;
}
```

### 7.3 Interactive Program

```c
// Software/C/userBDOS/calc.c

#include "bdos.h"

int main(void) {
    char buf[64];
    
    printf("Simple Calculator\n");
    printf("Enter expressions like: 5 + 3\n");
    printf("Type 'quit' to exit\n\n");
    
    while (1) {
        printf("> ");
        gets(buf);
        
        if (strcmp(buf, "quit") == 0) {
            break;
        }
        
        int a, b;
        char op;
        if (sscanf(buf, "%d %c %d", &a, &op, &b) == 3) {
            int result;
            switch (op) {
                case '+': result = a + b; break;
                case '-': result = a - b; break;
                case '*': result = a * b; break;
                case '/': result = (b != 0) ? a / b : 0; break;
                default:
                    printf("Unknown operator: %c\n", op);
                    continue;
            }
            printf("= %d\n", result);
        } else {
            printf("Invalid expression\n");
        }
    }
    
    printf("Goodbye!\n");
    return 0;
}
```

---

## 8. Program Building

### 8.1 Build Process

```bash
# Scripts/BCC/compile_user_program.sh

#!/bin/bash

PROGRAM=$1
OUTPUT=${2:-$PROGRAM}

# Compile with B32CC
./BuildTools/B32CC/smlrc \
    -DBDOS_USER \
    -I Software/C/libs/user \
    Software/C/userBDOS/${PROGRAM}.c \
    Software/ASM/Output/${PROGRAM}.asm

# Assemble with ASMPY
python3 ./BuildTools/ASMPY/asmpy/asmpy.py \
    Software/ASM/Output/${PROGRAM}.asm \
    Software/ASM/Output/${PROGRAM}.bin

# Upload to BDOS
python3 Scripts/Programmer/Network/netloader_client.py \
    192.168.1.100 \
    Software/ASM/Output/${PROGRAM}.bin \
    ${OUTPUT}
```

### 8.2 User Program Makefile

```makefile
# Software/C/userBDOS/Makefile

CC = ../../../BuildTools/B32CC/smlrc
AS = python3 ../../../BuildTools/ASMPY/asmpy/asmpy.py
UPLOAD = python3 ../../../Scripts/Programmer/Network/netloader_client.py

CFLAGS = -DBDOS_USER -I../libs/user
TARGET_IP ?= 192.168.1.100

PROGRAMS = hello dirlist calc editor

all: $(PROGRAMS)

%: %.c
	$(CC) $(CFLAGS) $< ../../ASM/Output/$@.asm
	$(AS) ../../ASM/Output/$@.asm ../../ASM/Output/$@.bin

upload-%: %
	$(UPLOAD) $(TARGET_IP) ../../ASM/Output/$*.bin $*

clean:
	rm -f ../../ASM/Output/*.asm ../../ASM/Output/*.bin
```

---

## 9. Background Process Support

### 9.1 Launching Background Processes

From stakeholder requirements:
> "I want to run background processes for basic network services"

```c
// kernel/shell/exec.c

int shell_run_background(const char* name) {
    int result = shell_run_program(name);
    if (result < 0) return result;
    
    // Don't make it foreground
    struct process* p = proc_get_by_name(name);
    if (p) {
        p->flags |= PROC_BACKGROUND;
    }
    
    term_puts("[");
    term_putint(p->pid);
    term_puts("] ");
    term_puts(name);
    term_puts(" started in background\n");
    
    return p->pid;
}

// Shell command: run program in background
void cmd_bg(int argc, char** argv) {
    if (argc < 2) {
        term_puts("Usage: bg <program>\n");
        return;
    }
    
    shell_run_background(argv[1]);
}
```

### 9.2 Foreground/Background Switching

```c
// Shell command: bring process to foreground
void cmd_fg(int argc, char** argv) {
    if (argc < 2) {
        // Bring most recent background process to foreground
        struct process* bg = proc_get_last_background();
        if (bg) {
            proc_set_foreground(bg);
            bg->flags &= ~PROC_BACKGROUND;
            term_puts(bg->name);
            term_puts("\n");
        } else {
            term_puts("No background processes\n");
        }
        return;
    }
    
    int pid = atoi(argv[1]);
    struct process* p = proc_get_by_pid(pid);
    if (p) {
        proc_set_foreground(p);
        p->flags &= ~PROC_BACKGROUND;
    } else {
        term_puts("No such process\n");
    }
}

// List background jobs
void cmd_jobs(int argc, char** argv) {
    int count = 0;
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        struct process* p = proc_get(i);
        if (p && (p->flags & PROC_BACKGROUND)) {
            term_puts("[");
            term_putint(p->pid);
            term_puts("] ");
            term_puts(p->name);
            term_puts("\n");
            count++;
        }
    }
    
    if (count == 0) {
        term_puts("No background jobs\n");
    }
}
```

---

## 10. Program Arguments (Future)

### 10.1 Passing Arguments to Programs

For future enhancement, support command-line arguments:

```c
// Argument passing via process structure
struct process {
    // ... existing fields
    
    int argc;
    char** argv;
    char arg_buffer[256];   // Storage for argument strings
};

// Parse and store arguments
void proc_set_args(struct process* p, int argc, char** argv) {
    p->argc = argc;
    
    // Copy arguments to process-local buffer
    char* ptr = p->arg_buffer;
    p->argv = (char**)(ptr);
    ptr += (argc + 1) * sizeof(char*);
    
    for (int i = 0; i < argc; i++) {
        p->argv[i] = ptr;
        strcpy(ptr, argv[i]);
        ptr += strlen(argv[i]) + 1;
    }
    p->argv[argc] = NULL;
}
```

---

## 11. Design Alternatives

### Alternative A: Shell as User Process

As discussed, shell could run in user space:

**Verdict**: Start with kernel shell for simplicity.

### Alternative B: Multiple Shells

Support multiple shell instances:

**Pros:**
- Multiple terminal sessions
- Better multi-tasking

**Cons:**
- More complex
- Overkill for single-user system

**Verdict**: Not needed for initial version.

### Alternative C: Script Support

Shell scripting capability:

**Pros:**
- Automation
- Batch processing

**Cons:**
- Significant implementation effort
- Parser complexity

**Verdict**: Nice to have for future.

---

## 12. Implementation Checklist

- [ ] Implement shell main loop
- [ ] Implement command parsing
- [ ] Implement command registration
- [ ] Implement help command
- [ ] Implement file commands (ls, cd, pwd, cat, mkdir, rm)
- [ ] Implement system commands (clear, echo, reboot)
- [ ] Implement process commands (ps, kill)
- [ ] Implement network commands (ifconfig, ping)
- [ ] Implement program loading
- [ ] Implement process startup
- [ ] Create user library (bdos.h)
- [ ] Implement syscall wrappers
- [ ] Implement printf for user programs
- [ ] Create example programs
- [ ] Test program loading and execution
- [ ] Test process switching (Alt-Tab)

---

## 13. Summary

| Component | Implementation | Notes |
|-----------|----------------|-------|
| Shell Location | In kernel | Simpler, direct access |
| Command Parsing | Space/quote aware | 16 args max |
| Built-in Commands | ~20 commands | Extensible |
| Program Location | /bin/ directory | Fixed path |
| Program Format | Raw binary | No headers |
| User Library | bdos.h | Syscall wrappers |
| Background Support | bg/fg commands | Process flag |
