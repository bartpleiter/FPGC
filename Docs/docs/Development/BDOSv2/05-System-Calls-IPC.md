# BDOS V2 System Calls & Piping

**Prepared by: Dr. Emily Watson (Compiler & Toolchain Expert)**  
**Contributors: Dr. Sarah Chen**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Software interrupt syscalls, removed IPC shared memory (use network for cluster), added piping*

---

## 1. Overview

This document describes the system call interface for BDOS V2. User programs use syscalls to request kernel services like file I/O, process management, and hardware access.

---

## 2. System Call Mechanism: Software Interrupt

### 2.1 Design Goals

1. **Clean Interface**: User programs should not know kernel internals
2. **Consistency**: All syscalls follow the same pattern
3. **Efficiency**: Minimal overhead for common operations
4. **Standard Approach**: Use software interrupt like x86

### 2.2 Recommended Approach: Software Interrupt Instruction

The cleanest syscall mechanism is a **software interrupt instruction**. This requires adding a new instruction to the B32P3 CPU.

**Proposed Instruction: `SOFTINT` (Software Interrupt)**

```
Encoding: SOFTINT imm8
Opcode:   TBD (suggest 0x3F or unused opcode)
Action:
    1. Save current PC to interrupt return register (or stack)
    2. Save current flags/status
    3. Jump to software interrupt vector (fixed address, e.g., 0x00000010)
    4. Kernel handles syscall
    5. Return via existing RETI instruction
```

### 2.3 CPU Modification Required

Add to B32P3 CPU:

```verilog
// In CPU instruction decoder
case (opcode)
    // ... existing opcodes ...
    
    OP_SOFTINT: begin
        // Push PC to stack or save to special register
        // Push flags/status
        // Set PC to SOFTWARE_INT_VECTOR (0x00000010)
        int_return_addr <= pc + 1;
        int_flags <= status_reg;
        pc <= SOFTWARE_INT_VECTOR;
        in_interrupt <= 1;
    end
endcase
```

### 2.4 Syscall Invocation

With software interrupt, syscalls become trivial:

```c
// User-side syscall function
static int syscall(int id, int a0, int a1, int a2, int a3) {
    // Arguments passed in registers:
    // r4 = syscall ID
    // r5 = arg0
    // r6 = arg1
    // r7 = arg2
    // r8 = arg3
    // Return value in r4
    
    register int r4 asm("r4") = id;
    register int r5 asm("r5") = a0;
    register int r6 asm("r6") = a1;
    register int r7 asm("r7") = a2;
    register int r8 asm("r8") = a3;
    
    asm volatile(
        "softint 0\n"       // Trigger software interrupt
        : "+r"(r4)          // r4 is both input (id) and output (result)
        : "r"(r5), "r"(r6), "r"(r7), "r"(r8)
        : "memory"
    );
    
    return r4;  // Return value in r4
}
```

### 2.5 Kernel-Side Handler

```c
// kernel/syscall/syscall.c
// Located at software interrupt vector (0x00000010)

void syscall_handler(void) {
    // Read arguments from registers
    int id   = get_reg(4);
    int arg0 = get_reg(5);
    int arg1 = get_reg(6);
    int arg2 = get_reg(7);
    int arg3 = get_reg(8);
    
    int result = -ENOSYS;  // Default: not implemented
    
    // Dispatch based on syscall ID
    switch (id) {
        // Process syscalls
        case SYS_EXIT:
            result = sys_exit(arg0);
            break;
        case SYS_GETPID:
            result = sys_getpid();
            break;
            
        // File syscalls
        case SYS_OPEN:
            result = sys_open((char*)arg0, arg1);
            break;
        case SYS_CLOSE:
            result = sys_close(arg0);
            break;
        case SYS_READ:
            result = sys_read(arg0, (void*)arg1, arg2);
            break;
        case SYS_WRITE:
            result = sys_write(arg0, (void*)arg1, arg2);
            break;
            
        // Terminal syscalls
        case SYS_PUTCHAR:
            result = sys_putchar(arg0);
            break;
        case SYS_GETCHAR:
            result = sys_getchar();
            break;
            
        // ... more cases ...
        
        default:
            result = -ENOSYS;
            break;
    }
    
    // Set return value in r4
    set_reg(4, result);
    
    // Return from interrupt (RETI instruction)
}
```

---

## 3. System Call Numbers

### 3.1 Syscall Categories

| Range | Category | Description |
|-------|----------|-------------|
| 0 | Reserved | Invalid syscall |
| 1-19 | Process | Process management |
| 20-39 | Memory | Memory operations |
| 40-79 | File System | File operations |
| 80-99 | I/O | Input/Output |
| 100-119 | Terminal | Console operations |
| 120-139 | Network | Raw Ethernet operations |
| 140-159 | Time | Timer and clock |
| 160-169 | Interrupt | Callback registration |
| 170-179 | Shell | Arguments and environment |

### 3.2 Complete Syscall Table

```c
// include/syscall.h

#ifndef SYSCALL_H
#define SYSCALL_H

// ============================================================================
// PROCESS SYSCALLS (1-19)
// ============================================================================
#define SYS_EXIT            1   // void exit(int status)
#define SYS_GETPID          2   // int getpid(void)
#define SYS_YIELD           3   // void yield(void)
#define SYS_SLEEP           4   // void sleep(unsigned int ms)
#define SYS_EXEC            5   // int exec(const char* path, char** argv)
#define SYS_GETPNAME        8   // int getpname(int pid, char* buf)

// ============================================================================
// MEMORY SYSCALLS (20-39)
// ============================================================================
#define SYS_SBRK            20  // void* sbrk(int increment)

// ============================================================================
// FILE SYSTEM SYSCALLS (40-79)
// ============================================================================
#define SYS_OPEN            40  // int open(const char* path, int flags)
#define SYS_CLOSE           41  // int close(int fd)
#define SYS_READ            42  // int read(int fd, void* buf, int count)
#define SYS_WRITE           43  // int write(int fd, void* buf, int count)
#define SYS_LSEEK           44  // int lseek(int fd, int offset, int whence)
#define SYS_STAT            45  // int stat(const char* path, struct stat* buf)
#define SYS_FSTAT           46  // int fstat(int fd, struct stat* buf)
#define SYS_MKDIR           47  // int mkdir(const char* path)
#define SYS_RMDIR           48  // int rmdir(const char* path)
#define SYS_UNLINK          49  // int unlink(const char* path)
#define SYS_RENAME          50  // int rename(const char* old, const char* new)
#define SYS_READDIR         51  // int readdir(int fd, struct dirent* entry)
#define SYS_GETCWD          52  // int getcwd(char* buf, int size)
#define SYS_CHDIR           53  // int chdir(const char* path)
#define SYS_SYNC            54  // void sync(void)
#define SYS_OPENDIR         55  // int opendir(const char* path)
#define SYS_CLOSEDIR        56  // int closedir(int fd)

// ============================================================================
// I/O SYSCALLS (80-99)
// ============================================================================
#define SYS_IOCTL           80  // int ioctl(int fd, int cmd, void* arg)

// ============================================================================
// TERMINAL SYSCALLS (100-119)
// ============================================================================
#define SYS_PUTCHAR         100 // int putchar(char c)
#define SYS_PUTS            101 // int puts(const char* s)
#define SYS_GETCHAR         102 // int getchar(void)
#define SYS_GETS            103 // char* gets(char* buf, int max) 
#define SYS_KBHIT           104 // int kbhit(void) - check if key available
#define SYS_CLRSCR          105 // void clrscr(void)
#define SYS_GOTOXY          106 // void gotoxy(int x, int y)
#define SYS_GETXY           107 // void getxy(int* x, int* y)
#define SYS_SETCOLOR        108 // void setcolor(int palette)

// ============================================================================
// NETWORK SYSCALLS (120-139) - Raw Ethernet
// ============================================================================
#define SYS_ETH_SEND        120 // int eth_send(unsigned char* frame, int len)
#define SYS_ETH_RECV        121 // int eth_recv(unsigned char* buf, int maxlen)
#define SYS_ETH_AVAIL       122 // int eth_available(void)
#define SYS_ETH_GETMAC      123 // void eth_getmac(unsigned char mac[6])
#define SYS_ETH_SETMAC      124 // void eth_setmac(unsigned char mac[6])

// ============================================================================
// TIME SYSCALLS (140-159)
// ============================================================================
#define SYS_TIME            140 // unsigned int time(void) - seconds since boot
#define SYS_MICROS          141 // unsigned int micros(void) - microseconds
#define SYS_USLEEP          142 // void usleep(unsigned int us)
#define SYS_ALARM           143 // int alarm(unsigned int ms, void (*fn)(void))
#define SYS_CANCELALARM     144 // void cancelalarm(int id)

// ============================================================================
// INTERRUPT SYSCALLS (160-169)
// ============================================================================
#define SYS_REGINT          160 // int regint(int intid, void (*handler)(int))
#define SYS_UNREGINT        161 // void unregint(int intid)

// ============================================================================
// SHELL/ARGUMENT SYSCALLS (170-179)
// ============================================================================
#define SYS_GETARGC         170 // int getargc(void)
#define SYS_GETARGV         171 // char** getargv(void)

// ============================================================================
// INPUT EVENT SYSCALLS (180-189)
// ============================================================================
#define SYS_INPUT_POLL      180 // int input_poll(struct input_event* event)
#define SYS_INPUT_PEEK      181 // int input_peek(void) - check if event waiting

#endif // SYSCALL_H
```

---

## 4. User-Side Syscall Library

### 4.1 Syscall Wrappers

```c
// libs/user/syscall.c

#include "syscall.h"

// Base syscall function (inline assembly)
static int _syscall(int id, int a0, int a1, int a2, int a3) {
    register int r4 asm("r4") = id;
    register int r5 asm("r5") = a0;
    register int r6 asm("r6") = a1;
    register int r7 asm("r7") = a2;
    register int r8 asm("r8") = a3;
    
    asm volatile("softint 0" : "+r"(r4) : "r"(r5), "r"(r6), "r"(r7), "r"(r8) : "memory");
    
    return r4;
}

// Convenience wrappers
void exit(int status) {
    _syscall(SYS_EXIT, status, 0, 0, 0);
    while(1);  // Never returns
}

int getpid(void) {
    return _syscall(SYS_GETPID, 0, 0, 0, 0);
}

int open(const char* path, int flags) {
    return _syscall(SYS_OPEN, (int)path, flags, 0, 0);
}

int close(int fd) {
    return _syscall(SYS_CLOSE, fd, 0, 0, 0);
}

int read(int fd, void* buf, int count) {
    return _syscall(SYS_READ, fd, (int)buf, count, 0);
}

int write(int fd, void* buf, int count) {
    return _syscall(SYS_WRITE, fd, (int)buf, count, 0);
}

int putchar(char c) {
    return _syscall(SYS_PUTCHAR, c, 0, 0, 0);
}

int puts(const char* s) {
    return _syscall(SYS_PUTS, (int)s, 0, 0, 0);
}

int getchar(void) {
    return _syscall(SYS_GETCHAR, 0, 0, 0, 0);
}

// Raw Ethernet
int eth_send(unsigned char* frame, int len) {
    return _syscall(SYS_ETH_SEND, (int)frame, len, 0, 0);
}

int eth_recv(unsigned char* buf, int maxlen) {
    return _syscall(SYS_ETH_RECV, (int)buf, maxlen, 0, 0);
}

int eth_available(void) {
    return _syscall(SYS_ETH_AVAIL, 0, 0, 0, 0);
}
```

---

## 5. Standard I/O Streams

### 5.1 Stream Abstraction

User programs use standard file descriptors:

```c
// Special file descriptors
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2
```

### 5.2 Kernel Stream Handling

```c
// In sys_read/sys_write, handle standard streams
int sys_read(int ufd, void* buf, int count) {
    // Validate user buffer
    if (validate_user_buffer(buf, count) < 0) {
        return -EFAULT;
    }
    
    if (ufd == STDIN_FILENO) {
        return read_stdin(buf, count);
    }
    
    // Normal file handling
    struct process* proc = proc_get_current();
    if (ufd < 3 || ufd >= 11 || proc->open_files[ufd - 3] < 0) {
        return -EBADF;
    }
    return fs_read(proc->open_files[ufd - 3], buf, count);
}

int sys_write(int ufd, void* buf, int count) {
    if (validate_user_buffer(buf, count) < 0) {
        return -EFAULT;
    }
    
    if (ufd == STDOUT_FILENO || ufd == STDERR_FILENO) {
        return write_stdout(buf, count);
    }
    
    // Normal file handling
    struct process* proc = proc_get_current();
    if (ufd < 3 || ufd >= 11 || proc->open_files[ufd - 3] < 0) {
        return -EBADF;
    }
    return fs_write(proc->open_files[ufd - 3], buf, count);
}
```

### 5.3 User-Side Print Function

**Note**: B32CC does not support variadic functions. Use single-argument print functions:

```c
// libs/user/stdio.c

// Print a string (no format specifiers)
int print(const char* s) {
    return puts(s);
}

// Print an integer
int print_int(int n) {
    char buf[12];
    int i = 11;
    int neg = 0;
    
    if (n < 0) {
        neg = 1;
        n = -n;
    }
    
    buf[i--] = '\0';
    
    do {
        buf[i--] = '0' + (n % 10);
        n = n / 10;
    } while (n > 0);
    
    if (neg) {
        buf[i--] = '-';
    }
    
    return puts(&buf[i + 1]);
}

// Print a hex value
int print_hex(unsigned int n) {
    char buf[11];
    char* hex = "0123456789ABCDEF";
    int i = 10;
    
    buf[i--] = '\0';
    
    do {
        buf[i--] = hex[n & 0xF];
        n = n >> 4;
    } while (n > 0);
    
    buf[i--] = 'x';
    buf[i--] = '0';
    
    return puts(&buf[i + 1]);
}
```

---

## 6. Piping Support

### 6.1 Design Philosophy

The stakeholder requested piping for sequential program execution:
> "I want the shell/terminal to support piping output of one program to the input of another"

Since BDOS V2 runs programs sequentially (not concurrently), piping uses temporary files:

1. Run program A, redirect stdout to temp file
2. Run program B, redirect stdin from temp file
3. Clean up temp file

### 6.2 Shell Implementation

```c
// kernel/shell/pipe.c

int shell_execute_pipe(const char* cmd1, const char* cmd2) {
    // Create temp file for pipe data
    const char* temp_path = "/tmp/pipe_data";
    
    // === Run first command ===
    // Save original stdout
    int orig_stdout = current_stdout_fd;
    
    // Open temp file for writing
    int temp_fd = fs_open(temp_path, O_WRITE | O_CREATE | O_TRUNC);
    if (temp_fd < 0) {
        print("pipe: failed to create temp file\n");
        return -1;
    }
    
    // Redirect stdout to temp file
    current_stdout_fd = temp_fd;
    
    // Execute first command
    int result1 = shell_execute_single(cmd1);
    
    // Restore stdout and close temp
    current_stdout_fd = orig_stdout;
    fs_close(temp_fd);
    
    if (result1 != 0) {
        fs_unlink(temp_path);
        return result1;
    }
    
    // === Run second command ===
    // Save original stdin
    int orig_stdin = current_stdin_fd;
    
    // Open temp file for reading
    temp_fd = fs_open(temp_path, O_READ);
    if (temp_fd < 0) {
        print("pipe: failed to read temp file\n");
        fs_unlink(temp_path);
        return -1;
    }
    
    // Redirect stdin from temp file
    current_stdin_fd = temp_fd;
    
    // Execute second command
    int result2 = shell_execute_single(cmd2);
    
    // Restore stdin and cleanup
    current_stdin_fd = orig_stdin;
    fs_close(temp_fd);
    fs_unlink(temp_path);
    
    return result2;
}
```

### 6.3 Multi-Stage Pipes

For `cmd1 | cmd2 | cmd3`:

```c
int shell_execute_pipeline(char** commands, int count) {
    int i;
    
    for (i = 0; i < count - 1; i++) {
        // Each stage writes to temp, next reads from temp
        char temp_in[32];
        char temp_out[32];
        
        // Alternate temp files
        if (i == 0) {
            temp_in[0] = '\0';  // First stage reads from real stdin
        } else {
            sprintf_simple(temp_in, "/tmp/pipe_%d", i - 1);
        }
        sprintf_simple(temp_out, "/tmp/pipe_%d", i);
        
        // Execute with redirects
        shell_execute_redirected(commands[i], temp_in, temp_out);
    }
    
    // Last command
    char temp_in[32];
    sprintf_simple(temp_in, "/tmp/pipe_%d", count - 2);
    shell_execute_redirected(commands[count - 1], temp_in, NULL);
    
    // Cleanup all temp files
    for (i = 0; i < count - 1; i++) {
        char temp[32];
        sprintf_simple(temp, "/tmp/pipe_%d", i);
        fs_unlink(temp);
    }
    
    return 0;
}
```

---

## 7. Interrupt Callback Registration

### 7.1 User Interrupt Handlers

Programs can register callback functions for hardware events:

```c
// Syscall
int sys_regint(int intid, void (*handler)(int)) {
    if (intid < 0 || intid >= MAX_USER_INTERRUPTS) {
        return -EINVAL;
    }
    
    struct process* proc = proc_get_current();
    
    // Store handler address in process context
    proc->int_handlers[intid] = (unsigned int)handler;
    
    // Register with kernel interrupt dispatcher
    kernel_register_user_callback(intid, current_pid, handler);
    
    return 0;
}

// Kernel interrupt dispatcher
void interrupt_dispatch(int intid) {
    // First, run kernel handlers
    kernel_handle_interrupt(intid);
    
    // Then, if a user process is running and has registered
    if (current_pid > 0) {
        struct process* proc = proc_get_current();
        if (proc->int_handlers[intid] != 0) {
            // Call user handler (in process context)
            void (*handler)(int) = (void (*)(int))proc->int_handlers[intid];
            handler(intid);
        }
    }
}
```

### 7.2 Available Interrupt IDs

```c
// include/interrupts.h

#define INT_TIMER1      0   // Timer 1 (system tick)
#define INT_TIMER2      1   // Timer 2 (USB polling)
#define INT_TIMER3      2   // Timer 3 (available for user)
#define INT_UART_RX     3   // UART receive ready
#define INT_UART_TX     4   // UART transmit ready
#define INT_SPI         5   // SPI transfer complete
#define INT_PS2         6   // PS/2 keyboard event
#define INT_ETH         7   // Ethernet packet received
#define INT_GPU_FRAME   8   // GPU frame complete (vsync)
```

---

## 8. Error Codes

```c
// include/errno.h

#define EOK         0       // Success
#define EPERM       -1      // Operation not permitted
#define ENOENT      -2      // No such file or directory
#define ESRCH       -3      // No such process
#define EIO         -5      // I/O error
#define ENXIO       -6      // No such device
#define EBADF       -9      // Bad file descriptor
#define EAGAIN      -11     // Try again
#define ENOMEM      -12     // Out of memory
#define EACCES      -13     // Permission denied
#define EFAULT      -14     // Bad address
#define EBUSY       -16     // Device busy
#define EEXIST      -17     // File exists
#define ENODEV      -19     // No such device
#define ENOTDIR     -20     // Not a directory
#define EISDIR      -21     // Is a directory
#define EINVAL      -22     // Invalid argument
#define EMFILE      -24     // Too many open files
#define ENOSPC      -28     // No space left
#define ENOSYS      -38     // Function not implemented
```

---

## 9. Syscall Performance

### 9.1 Software Interrupt Overhead

With proper hardware support, syscall overhead is minimal:

| Operation | Cycles | Notes |
|-----------|--------|-------|
| SOFTINT instruction | ~4 | Push PC, flags, jump |
| Argument passing | 0 | Already in registers |
| Dispatch switch | ~10-20 | Depends on syscall count |
| RETI instruction | ~4 | Pop and return |
| **Total overhead** | ~20-30 | Excluding actual work |

### 9.2 Comparison with Alternatives

| Method | Overhead | Clean Separation | Recommended |
|--------|----------|------------------|-------------|
| Software interrupt | ~20 cycles | ✅ Yes | ✅ |
| Function pointer | ~10 cycles | ❌ No boundary | For fallback |
| Memory-mapped frame | ~30 cycles | ⚠️ Partial | Old BDOS |

---

## 10. Implementation Checklist

- [ ] Add SOFTINT instruction to B32P3 CPU
- [ ] Set up software interrupt vector at 0x00000010
- [ ] Implement syscall dispatcher (`syscall_handler`)
- [ ] Implement process syscalls (exit, getpid, yield, exec)
- [ ] Implement file syscalls (open, close, read, write, lseek, stat)
- [ ] Implement terminal syscalls (putchar, puts, getchar, gets)
- [ ] Implement raw Ethernet syscalls (eth_send, eth_recv, eth_available)
- [ ] Implement time syscalls (time, micros, sleep, alarm)
- [ ] Implement interrupt callback registration
- [ ] Implement shell piping via temp files
- [ ] Create user-side syscall library (libs/user/syscall.c)
- [ ] Test all syscalls with simple programs

---

## 11. Summary

| Component | Implementation | Notes |
|-----------|----------------|-------|
| Syscall mechanism | Software interrupt | SOFTINT instruction (new) |
| Argument passing | Registers r4-r8 | Fast, no memory access |
| Return value | Register r4 | Standard convention |
| Syscall count | ~40 | Process, FS, Terminal, Network, Time |
| Piping | Temp file redirect | Sequential execution |
| Callbacks | Interrupt registration | For hardware events |
