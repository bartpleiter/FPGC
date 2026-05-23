# BDOS Context (for AI coding tools)

BDOS is the kernel/OS for FPGC. Built from separately-compiled C +
assembly sources using the modern toolchain (cproc → QBE → ASMPY →
flat binary). For the project-wide overview see
[Project-context.md](Project-context.md); for end-user docs see
[Docs/docs/Software/](../docs/Software/).

## Build & run

| Target | Purpose |
|--------|---------|
| `make compile-kernel` | Compile BDOS kernel (~224 KB binary) |
| `make run-kernel` | Compile + upload via UART |
| `make flash-kernel` | Flash to SPI (persistent) |
| `make compile-userbdos file=<name>` | Compile one userBDOS program |
| `make compile-userbdos-all` | Compile all ~35 userBDOS programs |

Pipeline: `crt0_kernel.asm + libc + libfpgc + kernel sources → cproc →
QBE → asm → ASMPY linker → .list → .bin`.

## Source layout

### libc (`Software/C/libc/`)

picolibc-derived freestanding C library: `string.h`, `stdlib.h`,
`stdio.h` (printf family), `ctype.h`, `stdint.h`, `errno.h`, ...
System hooks in `sys/`. printf goes through `_write` which the kernel
build wires to libterm.

### libfpgc (`Software/C/libfpgc/`)

Hardware abstraction. Headers in `include/`:

| Header | Purpose |
|--------|---------|
| `fpgc.h` | Memory map constants, MMIO addresses, SPI IDs, interrupt IDs |
| `sys.h` | `get_int_id()`, `get_boot_mode()`, `set_user_led()`, `get_micros()` |
| `timer.h` | 3 hardware timers + callbacks |
| `uart.h` | UART I/O (TX + RX ring) |
| `spi.h`, `spi_flash.h` | SPI bus + SPI-flash driver |
| `sd.h` | SD card SPI-mode driver (init, read/write blocks) |
| `dma.h` | DMA engine driver (7 modes, cache coherency) |
| `ch376.h` | CH376 USB-host driver (enumeration, HID, INT# pin) |
| `enc28j60.h` | ENC28J60 Ethernet driver |
| `gpu_hal.h`, `gpu_fb.h` | GPU HAL + framebuffer drawing API |
| `term.h` | libterm — terminal cell grid, ANSI parser, scrollback, alt screen, line discipline |
| `brfs.h` | BRFS v2 filesystem core |
| `brfs_storage.h` | Storage backend vtable |
| `brfs_storage_spi_flash.h` | SPI flash storage backend |
| `brfs_storage_sdcard.h` | SD card storage backend |
| `brfs_cache.h` | LRU cache layer (used by SD card backend) |
| `debug.h` | Hex-dump helpers |

### Kernel (`Software/C/kernel/`)

| File | Role |
|------|------|
| `include/kernel.h` | Master include — pulls libc, libfpgc, all kernel headers |
| `include/syscall_nums.h` | Syscall numbers (POSIX-aligned, clean v4 design) |
| `include/proc.h` | Process table, states, blocking, fd table |
| `include/vfs.h` | VFS core — file_ops vtable, open file table |
| `include/dev.h` | Device registration table |
| `include/mem.h` | Process memory allocator (free-list) |
| `include/hid.h` | USB keyboard subsystem, key constants, FIFO |
| `include/fs.h` | BRFS mount/format/sync helpers |
| `include/net.h` | Ethernet ring buffer, MAC, ISR |
| `include/fnp.h` | FNP protocol definitions |
| `src/main.c` | Entry point, interrupt handler, kernel loop |
| `src/init.c` | Hardware init (GPU, UART, timers, SPI, USB, Ethernet, FS) |
| `src/syscall.c` | Syscall C dispatcher (single switch) |
| `src/proc.c` | Process table, spawn, exit, waitpid, fd management |
| `src/sched.c` | FIFO scheduler, sleep/wake, context dispatch |
| `src/mem.c` | First-fit free-list allocator for process memory pool |
| `src/vfs.c` | VFS core (open/read/write/close dispatch, fd table) |
| `src/dev.c` | Device registration |
| `src/dev_tty.c` | /dev/tty (cooked/raw modes, line editing, UART mirror) |
| `src/dev_null.c` | /dev/null |
| `src/dev_pixpal.c` | /dev/pixpal GPU palette DAC |
| `src/dev_uart.c` | /dev/uart raw serial device |
| `src/dev_uart_mirror.c` | /dev/uart-mirror terminal UART mirror control |
| `src/dev_random.c` | /dev/random LFSR pseudo-random device |
| `src/dev_proc.c` | /proc virtual filesystem (uptime, meminfo, ps, df) |
| `src/fs.c` | BRFS mount/format/sync for SPI flash and SD card |
| `src/hid.c` | USB keyboard: INT# polling, HID translation, FIFO, key state |
| `src/net.c` | ENC28J60 Ethernet: ISR drain, ring buffer, MAC |
| `src/fnp.c` | FNP file-transfer protocol handler |

Each `.c` is compiled independently; standard
`#include <header.h>` with `-I` paths set by the build script.

### Shell (`Software/C/userBDOS/sh.c`)

The shell is a userland program, not part of the kernel. It implements:

- Bourne-style syntax: pipes, redirection, boolean chains, quoting
- Variable expansion: `$VAR`, `${VAR}`, `$?`, `$#`, `$0`–`$9`
- Built-in commands: help, clear, echo, cd, pwd, exit, halt, export,
  set, unset, env, true, false, test/[
- Control flow: if/then/else/fi, for/in/do/done, while/do/done
- Command history (up/down arrow) and tab completion
- Glob expansion: `*`, `?`
- External command lookup: `/bin/<name>` then cwd
- Script execution: `#!/bin/sh`

### Init (`Software/C/userBDOS/init.c`)

PID 1. Spawns `/bin/sh` in a loop, respawning on exit.

## Memory map (byte addresses)

| Region | Range | Size |
|--------|-------|------|
| Kernel code + BSS | `0x000000`–`0x0FFFFF` | 1 MiB |
| Kernel stacks (3) | `0x100000`–`0x10FFFF` | 64 KiB |
| Kernel heap | `0x110000`–`0x1FFFFF` | ~960 KiB |
| Process memory pool | `0x200000`–`0x1FFFFFF` | 30 MiB |
| BRFS SD cache | `0x2000000`–`0x23FFFFF` | 4 MiB (LRU) |
| BRFS SPI flash cache | `0x2400000`–`0x3FFFFFF` | 28 MiB |

Kernel stacks: main `0x107FFC`, syscall `0x10BFFC`, interrupt
`0x10FFFC`.

## Boot flow

1. `main()` → `kernel_init()`:
   - GPU init (VRAM clear, pattern table, palette)
   - libterm init (tile renderer + UART mirror callbacks)
   - Timer, UART, networking (ENC28J60 + FNP), USB keyboard init
   - Memory allocators: `kheap_init()` + `mem_init()`
   - Process table: `proc_init()` — 16 slots, PID 0 = kernel
   - VFS + device registration (all `/dev/*` devices)
   - Kernel stdio: fd 0/1/2 = `/dev/tty`
   - Filesystems: BRFS mount from SPI flash (`/`), SD card (`/sdcard`)
2. `proc_spawn("/bin/init", 0, 0)` — spawns init as PID 1
3. `sched_should_yield = 1` — trigger scheduler on first tick
4. `kernel_loop()` — polling loop: `hid_poll()`, `net_poll()`,
   `fnp_poll()`, `sched_tick()`

## Interrupts

`interrupt()` reads `INTID` and dispatches:

| INT ID | Source | Handler |
|--------|--------|---------|
| 1 | UART RX | Ring buffer fill |
| 2 | Timer 0 | Deferred ENC28J60 ISR retry (SPI was busy) |
| 3 | Timer 1 | USB keyboard HID report polling (10 ms periodic) |
| 4 | Timer 2 | `delay()` completion |
| 5 | Frame drawn | (unused) |
| 6 | ENC28J60 RX | Drain hardware RX into ring buffer |
| 7 | DMA complete | Transfer done notification |

**Ctrl+C:** Timer ISR detects Ctrl+C (ASCII 0x03), sets
`ctrl_c_pending` flag. Syscall dispatcher checks flag, forces
`proc_exit(130)` + `syscall_exit_to_kernel()`.

## Process model

Up to 16 processes with variable-size memory from a 30 MiB pool.
Cooperative multitasking with process blocking.

### Process states

- `PROC_FREE` (0): Slot unused
- `PROC_RUNNING` (1): Currently executing (one at a time)
- `PROC_READY` (2): Runnable, waiting for scheduler
- `PROC_BLOCKED` (3): Waiting on sleep, waitpid, or pipe I/O
- `PROC_ZOMBIE` (4): Exited, waiting for parent to collect

### Block reasons

- `BLOCK_SLEEP`: sleeping until `wake_time` microseconds
- `BLOCK_WAITPID`: waiting for child to exit
- `BLOCK_PIPE_READ` / `BLOCK_PIPE_WRITE`: pipe I/O (future)

### Process struct (key fields)

```c
struct proc {
    int pid, ppid, state, exit_code;
    unsigned int mem_base, mem_size;     /* contiguous memory region */
    unsigned int heap_base, heap_break;  /* sbrk heap tracking */
    unsigned int saved_regs[16];         /* r0–r15 */
    unsigned int saved_pc;
    unsigned int saved_hw_sp;            /* hardware stack pointer */
    unsigned int saved_hw_stack[256];    /* hardware stack contents */
    int fds[16];                         /* per-process fd table */
    char name[32], cwd[128];
    int argc; char *argv[32];
    int fg;                              /* owns terminal? */
    int blocked_reason;
    unsigned int wake_time;
    int wait_pid;
};
```

### Process lifecycle

1. **Spawn** (`SYS_SPAWN`): allocate memory, load binary + relocations,
   init registers/stack/heap, inherit parent fds + cwd, set READY
2. **Running**: scheduler picks via `context_enter()`, loads saved regs
3. **Blocking**: syscall sets BLOCKED + reason, `proc_was_blocked = 1`,
   asm layer exits to kernel loop
4. **Waking**: `sched_wake_sleepers()` or parent exit → READY
5. **Exit** (`SYS_EXIT`): close fds, free memory, ZOMBIE, wake parent
6. **Collection**: parent `waitpid` gets exit code, slot → FREE

### Memory layout per process

```
mem_base         → [Code + BSS]
                 → [Stack (256 KiB, grows DOWN)]
heap_base        → [Heap (grows UP via sbrk)]
mem_base+mem_size → End
```

Registers: `r13` (SP) = top of stack, `r14` (FP) = 0, `r15` = entry.

### Scheduler

FIFO scan of process table. `sched_tick()`:
1. Wake sleeping processes whose `wake_time` has passed
2. If `sched_should_yield` is set, pick next READY process
3. Mark current as READY, next as RUNNING
4. `context_enter()` loads user regs from proc struct, jumps to entry

## Input / HID

USB keyboard via CH376 over SPI. Hybrid polling approach:

- **Connect/disconnect**: main-loop polling of the CH376 INT# pin
  via `ch376_read_int()`.
- **HID report reading**: Timer 1 ISR callback (10 ms). When a
  keyboard is connected, reads the HID report and pushes events
  into the FIFO.
- **Boot-time enumeration**: `hid_init()` attempts connect + enumerate
  during boot.

Event FIFO: 64-entry ring; overflow drops new events. Consumers:
`hid_event_available()` / `hid_event_read()`.

Key encoding (FIFO and 4-byte `/dev/tty` packets):
- Printable ASCII: raw value
- Ctrl+A..Z: control codes 1..26
- Special keys: `KEY_*` constants (base `0x100`)

Real-time held-key bitmap (`hid_key_state`) rebuilt from raw HID
report each poll; user programs read via syscall `GET_KEY_STATE` (41).

## Filesystem

Two BRFS v2 instances, each backed by a different storage vtable:

| Instance | Backend | Mount point | Cache |
|----------|---------|------------|-------|
| `brfs_spi` | SPI flash 0 | `/` (root) | 28 MiB direct-mapped |
| `brfs_sd` | SD card (SPI5) | `/sdcard` | 4 MiB LRU |

- `fs_for_path(path)` routes paths starting with `/sdcard/` to
  `brfs_sd`; everything else goes to `brfs_spi`.
- VFS mount table: `ls /` shows `dev/`, `proc/`, `sdcard/` alongside
  BRFS entries.
- Sync is explicit (`brfs_sync()`).
- See [BRFS docs](../docs/Software/BRFS.md).

## VFS

`Software/C/kernel/src/vfs.c` — global open file table + device
dispatch.

### Device kinds

| Device | Path | Description |
|--------|------|-------------|
| file | (any BRFS path) | Byte-addressable BRFS entry |
| tty | `/dev/tty` | Terminal: cooked (line-buffered) or raw mode |
| null | `/dev/null` | Bit bucket |
| pixpal | `/dev/pixpal` | 256-entry GPU pixel-palette DAC |
| uart | `/dev/uart` | Raw UART serial TX/RX |
| uart-mirror | `/dev/uart-mirror` | Terminal UART mirror control |
| random | `/dev/random` | LFSR pseudo-random bytes |
| proc | `/proc/*` | Virtual files: uptime, meminfo, ps, df |

### Architecture

- **Global open file table**: 64 entries, each with refcount,
  `file_ops` vtable pointer, private data, flags, position.
- **Per-process fd table**: 16 fds mapping to global table indices.
- `fd_inherit(child, parent)`: copies fd table on spawn.
- `dup2` shares the global entry (increments refcount).

Every spawned program inherits `fd 0/1/2 = /dev/tty`.

## Networking (FNP)

Custom L2 protocol over ENC28J60 (EtherType `0xB4B4`).

- MAC derived from SPI flash 0 unique ID (`02:B4:B4:00:00:XX`).
- Message types: `FILE_START`/`FILE_DATA`/`FILE_END`/`FILE_ABORT`
  (file transfer with checksum + ACK/NACK), `KEYCODE` (remote
  keyboard input), `MKDIR` (create directory), `SYNC` (flush FS),
  `MESSAGE`.
- `fnp_poll()` runs each kernel-loop iteration.
- Ethernet ring buffer: 64-slot kernel-managed buffer, filled by ISR.
- User programs that call `NET_SEND`/`NET_RECV` read from the ring
  buffer directly; kernel FNP polling is paused.

## Memory allocators

### Kernel heap (bump allocator)

`kheap_init()` / `kheap_alloc(bytes)`. ~960 KiB region. Used for
kernel data structures (process table, fd table, free-list nodes).
`kheap_mark()` / `kheap_release()` for stack-like rewind.

### Process memory pool (first-fit free list)

`mem_init()` / `mem_alloc(size)` / `mem_free_region()`. 30 MiB pool.
32-byte aligned, up to 32 free-list nodes. Coalesces adjacent free
regions on release. `mem_grow_region()` supports in-place growth
(used by `sbrk`).

## Syscalls

ABI: `r4` = number, `r5`/`r6`/`r7` = up to 3 args, `r1` = return
value. Dispatched in `syscall_dispatch()` (`syscall.c`).

| # | Name | Args | Returns |
|---|------|------|---------|
| 1 | `EXIT` | `code` | *(no return)* |
| 2 | `YIELD` | — | 0 |
| 3 | `SPAWN` | `path, argc, argv` | pid / -1 |
| 4 | `WAITPID` | `pid` (-1=any child) | exit code |
| 5 | `GETPID` | — | pid |
| 6 | `KILL` | `pid` | 0 |
| 10 | `OPEN` | `path, flags` | fd |
| 11 | `CLOSE` | `fd` | 0 |
| 12 | `READ` | `fd, buf, bytes` | bytes read |
| 13 | `WRITE` | `fd, buf, bytes` | bytes written |
| 14 | `LSEEK` | `fd, off, whence` | new offset |
| 15 | `DUP2` | `oldfd, newfd` | newfd / -1 |
| 20 | `UNLINK` | `path` | 0 ok |
| 21 | `MKDIR` | `path` | 0 ok |
| 22 | `READDIR` | `path, buf, max` | entries |
| 23 | `RENAME` | `oldpath, newpath` | 0 ok |
| 24 | `STAT` | `path, stat_buf` | 0 ok |
| 25 | `SYNC` | — | 0 |
| 26 | `TRUNCATE` | `path, size` | 0 ok |
| 27 | `FORMAT` | `blocks, bpb, label` | 0 ok |
| 28 | `SD_FORMAT` | `blocks, bpb, label` | 0 ok |
| 30 | `CHDIR` | `path` | 0 |
| 31 | `GETCWD` | `buf, size` | buf pointer |
| 32 | `ARGC` | — | argc |
| 33 | `ARGV` | — | `char **argv` |
| 34 | `SBRK` | `incr` | old break / -1 |
| 40 | `SLEEP` | `ms` | 0 |
| 41 | `GET_KEY_STATE` | — | bitmap |
| 42 | `GET_TIME_US` | — | microseconds |
| 50 | `NET_SEND` | `buf, len` | len |
| 51 | `NET_RECV` | `buf, max_len` | bytes received |
| 52 | `NET_PACKET_COUNT` | — | count |
| 53 | `NET_GET_MAC` | `6-byte buf` | 0 |
| 60 | `PIPE` | `fildes[2]` | 0 ok |
| 61 | `IOCTL` | `fd, cmd, arg` | result |

`OPEN` flags: `O_RDONLY` (1), `O_WRONLY` (2), `O_RDWR` (3),
`O_APPEND` (4), `O_CREAT` (8), `O_TRUNC` (16), `O_RAW` (32),
`O_NONBLOCK` (64).

## Program execution

Programs are spawned via `SYS_SPAWN` (from shell or init):

1. `mem_alloc()` allocates contiguous region from pool
2. Binary read from BRFS into region
3. Relocation table applied (data pointers, load/loadhi pairs, jumps)
4. `kernel_ccache()` flushes L1 I/D caches
5. Register setup: `r13` = stack top, `r14` = 0, `r15` = entry
6. Process set to READY; scheduler dispatches via `context_enter()`
7. On `SYS_EXIT`: close fds, free memory, set ZOMBIE, wake parent

## Coding guidelines

- C11 with cproc/QBE limits: no inline asm, no `volatile`. Use
  `__builtin_load*` / `__builtin_store*` for MMIO.
- Each `.c` is compiled independently. Standard `#include <header.h>`.
- Assembly goes in dedicated `.asm` files.
- Timer 0 is free (deferred ENC28J60 retry). Timer 1 is HID polling.
  Timer 2 is `delay()`.
- DMA transfers must be 32-byte aligned. Call `cache_flush_data()`
  before MEM→device and after device→MEM.
- SD card is on SPI bus 5 (`FPGC_SPI_SD_CARD`). Use `sd.h` API.
- New device drivers: add in `src/dev_*.c`, register in `src/dev.c`.
- New syscalls: add number in `include/syscall_nums.h`, case in
  `syscall_dispatch()`, wrapper in `userlib/src/syscall.c`,
  prototype in `userlib/include/syscall.h`. Keep both in sync.
- Removed syscalls stay reserved (return `-1`).
