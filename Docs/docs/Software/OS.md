# OS (BDOS)

BDOS (Bart's Drive Operating System) is the custom operating system for the FPGC. It provides cooperative multitasking (up to 16 processes), a POSIX-aligned syscall interface, a virtual filesystem with device nodes, USB keyboard input, Ethernet networking, and the ability to run user programs. BDOS is loaded from SPI flash by the bootloader on startup and allows the FPGC to be used as a standalone computer.

BDOS is written in C using the [modern C toolchain](C-compiler.md) (cproc + QBE) and consists of 18 C source files, 1 context-switch assembly file, and links against the standard library (`libc`) and hardware abstraction library (`libfpgc`). The full build produces a ~224 KiB binary. It can be compiled and flashed with:

```bash
make compile-kernel   # Compile only
make run-kernel       # Compile and send via UART
make flash-kernel     # Compile and flash to SPI flash
```

!!! note
    In this documentation and throughout the codebase, BDOS is also referred to as the kernel.

## Memory Layout

BDOS organizes the FPGC's 64 MiB SDRAM into six regions:

| Address Range | Size | Description |
|---------------|------|-------------|
| `0x000000` – `0x0FFFFF` | 1 MiB | Kernel code, data, and BSS |
| `0x100000` – `0x10FFFF` | 64 KiB | Kernel stacks (3) |
| `0x110000` – `0x1FFFFF` | ~960 KiB | Kernel heap |
| `0x200000` – `0x1FFFFFF` | 30 MiB | Process memory pool |
| `0x2000000` – `0x23FFFFF` | 4 MiB | BRFS SD card cache (LRU) |
| `0x2400000` – `0x3FFFFFF` | 28 MiB | BRFS SPI flash cache |

### Kernel Stacks (0x100000)

Three hardware stacks grow downward within the 64 KiB region:

- **Main stack**: top at `0x107FFC` — used during boot and the kernel polling loop
- **Syscall stack**: top at `0x10BFFC` — switched to on every syscall entry
- **Interrupt stack**: top at `0x10FFFC` — used by the interrupt handler

### Kernel Heap (0x110000)

A bump allocator providing ~960 KiB of kernel-private memory. Used for internal data structures (process table, fd table, free-list nodes). Supports `kheap_alloc()`, `kheap_mark()` / `kheap_release()` for stack-like rewind.

### Process Memory Pool (0x200000)

A 30 MiB region managed by a first-fit free-list allocator. Each spawned process receives a contiguous allocation from this pool. Allocations are 32-byte aligned. Up to 32 free-list nodes track available regions, with automatic coalescing of adjacent free blocks on release. The allocator also supports in-place growth for `sbrk`.

## Process Model

BDOS supports up to 16 concurrent processes with cooperative multitasking. There is no preemption — processes yield control by making syscalls (e.g. `YIELD`, `SLEEP`, `WAITPID`, `EXIT`) or by performing blocking I/O.

### Process States

| State | Value | Meaning |
|-------|-------|---------|
| `FREE` | 0 | Slot unused |
| `RUNNING` | 1 | Currently executing (at most one) |
| `READY` | 2 | Runnable, waiting for the scheduler |
| `BLOCKED` | 3 | Waiting on sleep, waitpid, or pipe I/O |
| `ZOMBIE` | 4 | Exited, waiting for parent to collect exit code |

### Process Record

Each process carries:

- **PID / PPID**: process and parent process identifiers
- **Memory region**: base address and size within the process pool
- **Heap**: per-process heap managed via `sbrk` (grows upward from after the stack)
- **Saved registers**: full register set (`r0`–`r15`) and program counter, saved/restored on context switch
- **File descriptors**: up to 16 per process, inherited from parent on spawn
- **Working directory**: per-process cwd (up to 128 characters)
- **Arguments**: argc + argv (up to 32 arguments)
- **Foreground flag**: whether the process owns the terminal
- **Block info**: block reason, wake time (for sleep), target PID (for waitpid)

### Memory Layout Per Process

```
mem_base         → [Code + BSS]
                   [Stack (256 KiB, grows DOWN from top)]
heap_base        → [Heap (grows UP via sbrk)]
mem_base+mem_size → End of allocation
```

Registers on entry: `r13` (SP) = top of stack, `r14` (FP) = 0, `r15` = entry point.

### Scheduler

The scheduler uses FIFO scanning of the process table. On each `sched_tick()`:

1. Wake any sleeping processes whose `wake_time` has passed
2. If a yield has been requested (`sched_should_yield`), find the next READY process
3. Save the current process's state, mark it READY
4. Mark the next process RUNNING, load its registers via `context_enter()`

### Ctrl+C

When the USB keyboard ISR detects Ctrl+C (ASCII 0x03), it sets a `ctrl_c_pending` flag. The syscall dispatcher checks this flag on every syscall entry and forces `proc_exit(130)` on the foreground process.

## Shell

The shell (`/bin/sh`) is a userland program — not part of the kernel. It is spawned by `/bin/init` (PID 1) and respawned automatically when it exits. See [Shell.md](Shell.md) for the full syntax reference and built-in list.

## Virtual File System (VFS)

All I/O goes through the VFS layer (`Software/C/kernel/src/vfs.c`). The VFS maintains a global open file table (64 entries) and dispatches operations through per-device `file_ops` vtables.

### Devices

| Device | Path | Description |
|--------|------|-------------|
| file | *(any BRFS path)* | Byte-addressable BRFS entry; honours `O_CREAT`, `O_TRUNC`, `O_APPEND` |
| tty | `/dev/tty` | Terminal device: cooked mode (line-buffered) by default. Pass `O_RAW` for 4-byte key-event packets; combine with `O_NONBLOCK` for polling |
| null | `/dev/null` | Discards writes, returns EOF on reads |
| pixpal | `/dev/pixpal` | 256-entry GPU pixel-palette DAC (1024 bytes, `0x00RRGGBB`). `lseek` sets byte cursor; writes autoincrement one entry |
| uart | `/dev/uart` | Raw UART serial TX/RX |
| random | `/dev/random` | LFSR pseudo-random bytes |
| proc | `/proc/*` | Virtual files: `uptime`, `meminfo`, `ps`, `df` |

Every spawned process inherits `fd 0/1/2 = /dev/tty`, so `printf` / `puts` / `sys_write(1, ...)` route through the terminal driver. Redirection and pipes work for any program that uses standard I/O.

### File Operations

Programs interact with files through POSIX-style syscalls: `OPEN`, `READ`, `WRITE`, `CLOSE`, `LSEEK`, `DUP2`. The VFS routes each operation to the appropriate device's vtable implementation. `DUP2` shares the global open file entry (increments refcount).

## Program Loading and Execution

Programs are spawned via the `SYS_SPAWN` syscall (from the shell, init, or any process):

1. **Memory allocation**: `mem_alloc()` allocates a contiguous region from the process pool
2. **Binary read**: The file is read from BRFS into the allocated region
3. **Relocation**: The relocation table is applied, patching all absolute address references (data pointers, `load`+`loadhi` pairs, jump targets) by adding the region's base address. See [Assembler — Relocatable Code](Assembler.md#relocatable-code) for the binary layout
4. **Cache flush**: `kernel_ccache()` clears L1I/L1D caches
5. **Register setup**: `r13` = stack top, `r14` = 0, `r15` = entry point
6. **Process state**: set to READY; the scheduler dispatches via `context_enter()`
7. **Exit**: on `SYS_EXIT`, fds are closed, memory is freed, process becomes ZOMBIE, parent is woken

### Per-Process Heap

Each process has its own heap managed via the `SBRK` syscall. The heap starts after the stack and grows upward. The kernel tracks `heap_base` and `heap_break` per process and uses `mem_grow_region()` to extend the allocation in-place when possible. All heap memory is freed when the process exits.

## Syscalls

User programs communicate with the kernel through a software syscall mechanism. Since B32P3 has no dedicated trap instruction, syscalls are implemented as jumps to a fixed kernel entry point at address 12 (byte offset `0xC`).

### Mechanism

1. User calls `syscall(num, a1, a2, a3)` — the C ABI places arguments in `r4`–`r7`
2. The wrapper saves `r15`, loads address 12, sets the return address, and jumps
3. Address 12 contains `jump Syscall`, redirecting into the kernel's assembly trampoline
4. The trampoline saves all user registers to the process struct, switches to the syscall stack, and calls the C dispatcher
5. The C dispatcher handles the request, returns a result in `r1`
6. The trampoline restores user registers and returns to the user program

### Syscall Table

| # | Name | Arguments | Returns | Description |
|---|------|-----------|---------|-------------|
| 1 | `EXIT` | `code` | *(no return)* | Terminate process |
| 2 | `YIELD` | — | 0 | Yield to scheduler |
| 3 | `SPAWN` | `path, argc, argv` | pid / -1 | Spawn a new process |
| 4 | `WAITPID` | `pid` (-1=any) | exit code | Wait for child to exit |
| 5 | `GETPID` | — | pid | Get current process ID |
| 6 | `KILL` | `pid` | 0 | Kill a process |
| 10 | `OPEN` | `path, flags` | fd | Open a file or device |
| 11 | `CLOSE` | `fd` | 0 | Close a file descriptor |
| 12 | `READ` | `fd, buf, bytes` | bytes read | Read from fd |
| 13 | `WRITE` | `fd, buf, bytes` | bytes written | Write to fd |
| 14 | `LSEEK` | `fd, off, whence` | new offset | Seek within fd |
| 15 | `DUP2` | `oldfd, newfd` | newfd / -1 | Duplicate fd |
| 20 | `UNLINK` | `path` | 0 | Delete file |
| 21 | `MKDIR` | `path` | 0 | Create directory |
| 22 | `READDIR` | `path, buf, max` | entries | List directory entries |
| 23 | `RENAME` | `old, new` | 0 | Rename file |
| 24 | `STAT` | `path, buf` | 0 | Get file info |
| 25 | `SYNC` | — | 0 | Flush filesystem to storage |
| 30 | `CHDIR` | `path` | 0 | Change working directory |
| 31 | `GETCWD` | `buf, size` | pointer | Get working directory |
| 32 | `ARGC` | — | argc | Get argument count |
| 33 | `ARGV` | — | `char **argv` | Get argument vector |
| 34 | `SBRK` | `incr` | old break / -1 | Grow process heap |
| 40 | `SLEEP` | `ms` | 0 | Sleep for milliseconds |
| 41 | `GET_KEY_STATE` | — | bitmap | Held-key bitmap |
| 42 | `GET_TIME_US` | — | microseconds | Microsecond timestamp |
| 50 | `NET_SEND` | `buf, len` | len | Send Ethernet frame |
| 51 | `NET_RECV` | `buf, max_len` | bytes | Receive Ethernet frame |
| 52 | `NET_PACKET_COUNT` | — | count | Queued RX packets |
| 53 | `NET_GET_MAC` | `6-byte buf` | 0 | Get MAC address |
| 60 | `PIPE` | `fildes[2]` | 0 | Create a pipe |
| 61 | `IOCTL` | `fd, cmd, arg` | result | Device-specific control |

### Open Flags

| Flag | Value | Meaning |
|------|-------|---------|
| `O_RDONLY` | 1 | Read only |
| `O_WRONLY` | 2 | Write only |
| `O_RDWR` | 3 | Read/write |
| `O_APPEND` | 4 | Append mode |
| `O_CREAT` | 8 | Create if not exists |
| `O_TRUNC` | 16 | Truncate on open |
| `O_RAW` | 32 | Raw mode (for `/dev/tty`) |
| `O_NONBLOCK` | 64 | Non-blocking I/O |

### User-Side Library

User programs link against the syscall library (`Software/C/userlib/`), which provides convenience wrappers:

```c
#include <syscall.h>

int main(void)
{
    sys_putstr("Hello from userBDOS!\n");      /* sys_write(1, ...) */
    sys_write(2, "debug trace\n", 12);         /* mirrored to UART  */
    return 0;
}
```

For raw key events (games), open `/dev/tty` in raw mode:

```c
int fd = sys_tty_open_raw(1 /* nonblocking */);
int ev;
while ((ev = sys_tty_event_read(fd, 0)) >= 0) { /* handle key */ }
sys_close(fd);
```

**Network ownership:** The first call to `NET_SEND` or `NET_RECV` takes ownership of the Ethernet controller away from the kernel's FNP protocol handler. While a user program owns the network, the kernel will not consume incoming packets. Ownership is released on exit.

## Interrupt Handling

BDOS owns all hardware interrupts. The interrupt vector at address 4 points to the interrupt handler, which saves all registers to the interrupt stack, reads the interrupt ID, dispatches to the appropriate handler, restores registers, and returns via `reti`.

| INT ID | Source | Handler |
|--------|--------|---------|
| 1 | UART RX | Ring buffer fill |
| 2 | Timer 0 | Deferred Ethernet ISR retry (SPI was busy) |
| 3 | Timer 1 | USB keyboard HID report polling (10 ms) |
| 4 | Timer 2 | `delay()` completion |
| 5 | Frame Drawn | *(unused)* |
| 6 | ENC28J60 RX | Drain packets into ring buffer |
| 7 | DMA complete | Transfer done notification |

The Ethernet interrupt (INT 6) is the primary network reception path. When a packet arrives, the ISR drains all pending packets from the ENC28J60 into a 64-slot kernel ring buffer. If the SPI bus is busy during an interrupt, the ISR defers by starting a 1 ms timer on Timer 0, which retries the drain.

## USB Keyboard Input

USB keyboard input uses a hybrid polling approach via the CH376 USB host controller over SPI:

- **Connect/disconnect**: polled in the kernel main loop via `hid_poll()`, checking the CH376 INT# pin
- **HID reports**: read by a Timer 1 ISR callback every 10 ms while a keyboard is connected
- **Key events**: pushed into a 64-entry FIFO ring buffer; consumers use `hid_event_available()` / `hid_event_read()`
- **Held-key state**: real-time bitmap rebuilt from each HID report, readable via `GET_KEY_STATE` syscall

## Filesystem

Two BRFS v2 instances are mounted, each backed by a different storage device:

| Instance | Backend | Mount Point | Cache |
|----------|---------|-------------|-------|
| SPI flash | SPI flash chip 0 | `/` (root) | 28 MiB direct-mapped |
| SD card | SD card (SPI bus 5) | `/sdcard` | 4 MiB LRU |

Path routing: paths starting with `/sdcard/` go to the SD card instance; everything else goes to SPI flash. The VFS mount table shows `dev/`, `proc/`, and `sdcard/` alongside BRFS entries when listing `/`.

Filesystem sync is explicit — call `SYNC` or run the `sync` command to flush changes to storage. See [BRFS](BRFS.md) for filesystem details.

## Networking

BDOS uses interrupt-driven packet reception via the ENC28J60 Ethernet controller. The MAC address is derived from the SPI flash chip's unique ID (`02:B4:B4:00:00:XX`).

The kernel runs the FNP (FPGC Network Protocol) handler, a custom L2 protocol (EtherType `0xB4B4`) supporting file transfers and remote keyboard input. `fnp_poll()` processes packets each kernel loop iteration. User programs can take network ownership via `NET_SEND`/`NET_RECV` syscalls for raw Ethernet access.

See [FNP](FNP.md) for protocol details.

## Initialization

On boot, BDOS:

1. Initializes hardware: GPU (VRAM, pattern table, palette), libterm (terminal renderer + UART mirror), timers, UART, Ethernet (ENC28J60 + FNP), USB keyboard, SPI
2. Initializes memory allocators: kernel heap (`kheap_init`) and process pool (`mem_init`)
3. Initializes process table (`proc_init`): 16 slots, PID 0 reserved for kernel
4. Registers VFS devices: `/dev/tty`, `/dev/null`, `/dev/pixpal`, `/dev/uart`, `/dev/random`, `/proc/*`
5. Opens kernel stdio: fd 0/1/2 = `/dev/tty`
6. Mounts BRFS from SPI flash (`/`) and optionally SD card (`/sdcard`)
7. Spawns `/bin/init` as PID 1 (which in turn spawns `/bin/sh`)
8. Enters `kernel_loop()`: polling loop running `hid_poll()`, `net_poll()`, `fnp_poll()`, `sched_tick()`
