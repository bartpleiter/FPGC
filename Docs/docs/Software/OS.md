# OS (BDOS)

BDOS (Bart's Drive Operating System) is the custom operating system for the FPGC. It provides a shell, filesystem access, hardware drivers (Ethernet, USB/HID input, etc), and most importantly the ability to run user programs while providing system calls. It acts as a single program that is meant to be loaded from SPI flash by the bootloader on startup (depending on the boot switch), and allows the FPGC to be used as a standalone computer without needing to connect it to a host PC.

BDOS is written in C using the [modern C toolchain](C-compiler.md) (cproc + QBE) and consists of 13 C source files, 1 context-switch assembly file, and links against the standard library (`libc`) and hardware abstraction library (`libfpgc`). The full build produces a ~200 KiB binary from 37 source files. It can be compiled and flashed with:

```bash
make compile-bdos   # Compile only
make run-bdos       # Compile and send via UART
make flash-bdos     # Compile and flash to SPI flash
```

!!! note
    In this documentation and throughout the codebase, BDOS is also sometimes referred to as the kernel.

## Memory Layout

BDOS organizes the FPGC's 64 MiB SDRAM into four regions:

| Address Range | Size | Description |
|---------------|------|-------------|
| `0x000000` – `0x3FFFFF` | 4 MiB | Kernel code, data, and stacks |
| `0x400000` – `0x1FFFFFF` | 28 MiB | Kernel heap (dynamic allocation) |
| `0x2000000` – `0x2FFFFFF` | 16 MiB | User program slots |
| `0x3000000` – `0x3FFFFFF` | 16 MiB | BRFS filesystem cache |

### Kernel Region (0x000000)

Contains the BDOS binary (code + data). Three stacks grow downward from the top:

- **Main stack**: top at `0x3DFFFC`
- **Syscall stack**: top at `0x3EFFFC`
- **Interrupt stack**: top at `0x3FFFFC`

### User Program Region (0x2000000)

Divided into 8 slots of 2 MiB each. Programs are compiled with the modern C toolchain and assembled with ASMPY using `-h -i` flags, which produces a relocatable binary with a relocation table. BDOS loads the binary into a slot and applies relocations at load time. The stack is placed at the top of the allocated slot. This allows up to 8 concurrent user programs, although that would require a scheduler to manage them, which has yet to be implemented. Programs with lots of data should use the heap for dynamic allocation, and load data from BRFS into the heap at runtime.

| Slot | Address Range | Stack Top |
|------|---------------|-----------|
| 0 | `0x2000000` – `0x21FFFFF` | `0x21FFFFC` |
| 1 | `0x2200000` – `0x23FFFFF` | `0x23FFFFC` |
| 2 | `0x2400000` – `0x25FFFFF` | `0x25FFFFC` |
| ... | ... | ... |
| 7 | `0x2E00000` – `0x2FFFFFF` | `0x2FFFFFC` |

## Shell

BDOS ships with a Bourne-style interactive shell (the v2 shell, landed by the
[shell-terminal-v2 plan](../../plans/shell-terminal-v2.md)). It supports
quoting, pipes (`|`, implemented over temporary files), redirection
(`<`, `>`, `>>`), boolean chains (`&&`, `||`, `;`), variable expansion
(`$VAR`, `${VAR}`), and `#!/bin/sh` script execution.

Commonly used built-ins:

| Command | Description |
|---------|-------------|
| `help` | List available built-in commands |
| `clear` | Clear the terminal screen |
| `echo <text>` | Print text to the terminal |
| `pwd` / `cd <path>` | Working-directory query / change |
| `ls [path]` | List directory contents |
| `cat <file>` / `write <file> <text>` | File read / overwrite |
| `mkdir`, `mkfile`, `rm`, `cp`, `mv` | File-tree manipulation |
| `df` / `sync` | Filesystem usage / flush to flash |
| `jobs`, `fg <id>`, `kill <id>` | Job control over user programs |
| `export`, `set`, `unset`, `env` | Environment / shell variables |
| `exit`, `true`, `false` | Process control & test helpers |

Anything that is not a built-in is resolved as a program: bare names look in
`/bin/<name>` first, then the cwd; names containing `/` or starting with `.`
are resolved as paths. The previously-built-in `format` command was moved to
the standalone binary `/bin/format` in Phase E (the boot-time mount-failure
wizard still lives inside the kernel for first-boot recovery).

For the full syntax reference and the up-to-date built-in list, see
[Shell.md](Shell.md). For terminal capabilities (libterm, supported ANSI
escapes), see [Terminal.md](Terminal.md).

## Process model

BDOS runs at most one foreground program plus a small number of suspended
background jobs in the eight 2 MiB user slots (no preemption, no concurrent
execution). Each running program has a process record in the PID table
(`Software/C/bdos/proc.c`) carrying:

- A monotonically increasing **PID** (user-visible; `jobs` / `fg` / `kill`
  operate on PIDs).
- Its **slot index** (0–7) and base address.
- A **per-process file-descriptor table** (default `fd 0/1/2 = /dev/tty`,
  inherited from the shell, redirected by `<`/`>`/`>>`/`|`).
- A **per-process argv arena** allocated on the kernel heap and freed on exit
  (so the child cannot corrupt the shell's buffers and so leaks across
  invocations are impossible).

Heap allocations made by a program through `HEAP_ALLOC` are also released as a
group on exit — see the Heap section above.

## Virtual file system (VFS)

User-facing byte I/O goes through the VFS layer in
`Software/C/bdos/vfs.c` rather than directly into BRFS. The VFS owns the
per-process fd table and exposes a uniform `open` / `read` / `write` /
`close` / `lseek` / `dup2` API over five device kinds:

| Device   | Backing | Notes |
|----------|---------|-------|
| `file`   | BRFS v2 entry | Byte-addressable view over the word-oriented filesystem; honours `O_CREAT`, `O_TRUNC`, `O_APPEND`. |
| `tty`    | libterm + keyboard FIFO | `/dev/tty`. Cooked mode by default (line-buffered, ANSI emit on writes). Pass `O_RAW` to receive 4-byte little-endian key-event packets per `read`; combine with `O_NONBLOCK` for polling games. |
| `pipe`   | Temp file under `/tmp/` | Rewritten by the shell from `a \| b` into `a >/tmp/p.N ; b </tmp/p.N`. No concurrency required by the execution model. |
| `null`   | — | `/dev/null`. Discards writes, returns EOF on reads. |
| `pixpal` | 256-entry × 24-bit RGB pixel-palette DAC | `/dev/pixpal`. 1024-byte fixed-size device (256 × 4 bytes, `0x00RRGGBB` LE). `lseek` sets the byte cursor; `write` autoincrements one entry at a time, mirroring the MS-DOS / VGA DAC port model. Both length and cursor must be 4-byte aligned. |

`fd 0`, `fd 1`, and `fd 2` are pre-opened to `/dev/tty` for every spawned
program. `printf` / `puts` / `sys_write(1, ...)` therefore route through the
TTY driver, which means redirection and pipes work for *any* program that
uses the standard I/O wrappers — no per-program changes are needed.

## Program Loading and Execution

Programs are launched by typing their name or path at the shell prompt. The dispatcher tries built-in commands first, then program resolution:

1. If the name contains `/` or starts with `.`: resolved as a path (absolute or relative to cwd)
2. Bare names: try `/bin/<name>` first, then fall back to cwd

Before execution, the shell stores `argc` and `argv` in kernel globals so the program can retrieve them via the `SHELL_ARGC` and `SHELL_ARGV` syscalls.

### Loading Process

1. **Path resolution**: If the name has no `/`, BDOS looks in `/bin/` automatically, then falls back to cwd
2. **Slot allocation**: A free slot is allocated via `bdos_slot_alloc()`
3. **Read binary**: The file is read from BRFS in 256-word chunks into the slot's memory region
4. **Apply relocations**: BDOS reads `program_size` from header word 2. If the file is larger than `program_size`, a relocation table is present after the program data. The loader reads the table and patches every absolute address reference by adding the slot's base address. This handles data pointers (`.int label`), `load`+`loadhi` address-loading instruction pairs, and header `jump` instructions. See [Assembler — Relocatable Code](Assembler.md#relocatable-code) for the binary layout and entry encoding.
5. **Cache flush**: The `ccache` instruction clears L1I/L1D caches to ensure the CPU fetches fresh instructions
6. **Register setup**:
      - `r13` (stack pointer) = top of slot
      - `r15` (return address) = BDOS return trampoline
7. **Jump**: Execution transfers to offset 0 of the slot, which contains the header `jump Main` (now relocated to the correct absolute address)
8. **Return**: When the program's `main()` returns (via `r15`), BDOS restores its own registers, frees the slot (including heap cleanup), and prints the exit code

## Heap

The kernel heap region (`0x400000`–`0x1FFFFFF`, 28 MiB) is managed by a simple bump allocator. User programs allocate memory via the `HEAP_ALLOC` syscall. All allocations are freed together when the program exits, is killed, or encounters an error — there is no individual `free()`.

This design is intentional: the bump allocator is very fast and simple, and the per-program cleanup ensures no memory leaks across program invocations. Programs that need to "reallocate" a buffer simply allocate a new, larger block and copy the data over; the old block is wasted but freed on exit.

## Syscalls

User programs communicate with BDOS through a software syscall mechanism, as there is no dedicated trap or software interrupt instruction (I could not figure out how to make that work, as I do not want to run syscall code within an interrupt, blocking other interrupts from happening). Therefore, syscalls are implemented as regular jumps to a fixed kernel entry point.

### Mechanism

The ASMPY assembler header places a `jump Syscall` instruction at absolute address 12 (byte offset `0xC`, as part of the header, enabled with the `-s` flag). User programs invoke a syscall by jumping to this address with arguments in registers. The flow is:

1. User calls `syscall(num, a1, a2, a3)`, the C ABI places arguments in `r4` to `r7`
2. The inline assembly saves `r15`, loads address 12 into a temp register, sets the return address via `savpc`/`add`, and jumps
3. Address 12 contains `jump Syscall`, redirecting into the kernel's assembly trampoline
4. The trampoline saves all registers (except `r1`) to the hardware stack, switches to the kernel syscall stack, and calls the C dispatcher
5. The C dispatcher handles the request, returns a result in `r1`
6. The trampoline restores registers and returns to the user program

### Available syscalls

!!! note
    See [`bdos_syscall.h`](https://github.com/b4rt-dev/FPGC/blob/main/Software/C/bdos/include/bdos_syscall.h)
    and [`syscall.h`](https://github.com/b4rt-dev/FPGC/blob/main/Software/C/userlib/include/syscall.h)
    for the up-to-date list of syscall numbers and userland wrappers.
    Slot numbers retired in shell-terminal-v2 Phase E are commented out in
    `bdos_syscall.h` and the dispatcher returns `-1` for them.

| Number | Name | Arguments | Returns | Description |
|--------|------|-----------|---------|-------------|
| 4  | `FS_OPEN`         | `a1` = path                                           | brfs fd          | Open a BRFS entry (raw word-oriented API; prefer `OPEN`) |
| 5  | `FS_CLOSE`        | `a1` = fd                                             | 0 ok             | Close a BRFS fd |
| 6  | `FS_READ`         | `a1` = fd, `a2` = buf, `a3` = words                   | words read       | Word-oriented BRFS read |
| 7  | `FS_WRITE`        | `a1` = fd, `a2` = buf, `a3` = words                   | words written    | Word-oriented BRFS write |
| 8  | `FS_SEEK`         | `a1` = fd, `a2` = word offset                         | 0 ok             | BRFS seek |
| 9  | `FS_STAT`         | `a1` = path, `a2` = `brfs_dir_entry *`                | 0 ok             | Stat a path |
| 10 | `FS_DELETE`       | `a1` = path                                           | 0 ok             | Delete a file or empty dir |
| 11 | `FS_CREATE`       | `a1` = path                                           | 0 ok             | Create an empty file |
| 12 | `FS_FILESIZE`     | `a1` = fd                                             | size in words    | Size of an open BRFS file |
| 13 | `SHELL_ARGC`      | —                                                     | argc             | Argument count for current process |
| 14 | `SHELL_ARGV`      | —                                                     | `char **argv`    | Argument vector pointer |
| 15 | `SHELL_GETCWD`    | —                                                     | `char *cwd`      | Shell's current working directory |
| 20 | `HEAP_ALLOC`      | `a1` = size in words                                  | pointer / 0      | Allocate from kernel heap (released on exit) |
| 21 | `DELAY`           | `a1` = milliseconds                                   | 0                | Sleep for the given number of ms |
| 23 | `EXIT`            | `a1` = exit code                                      | *(no return)*    | Terminate calling process |
| 24 | `FS_READDIR`      | `a1` = path, `a2` = `brfs_dir_entry *`, `a3` = max    | entries returned | Enumerate a directory |
| 25 | `GET_KEY_STATE`   | —                                                     | bitmap           | Held-key bitmap (see `KEYSTATE_*`) |
| 27 | `NET_SEND`        | `a1` = buffer, `a2` = length                          | 1 ok / 0 err     | Send raw Ethernet frame (takes net ownership) |
| 28 | `NET_RECV`        | `a1` = buffer, `a2` = max length                      | bytes received   | Pop a packet from RX ring (takes net ownership) |
| 29 | `NET_PACKET_COUNT`| —                                                     | count            | Packets queued in RX ring |
| 30 | `NET_GET_MAC`     | `a1` = 6-int buffer                                   | 0                | Copy our MAC address |
| 33 | `FS_MKDIR`        | `a1` = path                                           | 0 ok             | Create a directory |
| 34 | `OPEN`            | `a1` = path, `a2` = flags (`O_RDONLY` … `O_RAW` …)    | fd               | VFS `open()` (file / `/dev/tty` / `/dev/null` / pipe) |
| 35 | `READ`            | `a1` = fd, `a2` = buf, `a3` = bytes                   | bytes read       | Byte-oriented VFS read |
| 36 | `WRITE`           | `a1` = fd, `a2` = buf, `a3` = bytes                   | bytes written    | Byte-oriented VFS write |
| 37 | `CLOSE`           | `a1` = fd                                             | 0 ok             | VFS close |
| 38 | `LSEEK`           | `a1` = fd, `a2` = offset, `a3` = whence               | new offset       | VFS lseek |
| 39 | `DUP2`            | `a1` = oldfd, `a2` = newfd                            | newfd / -1       | Duplicate a fd; used by the shell for `<`/`>`/`|` |
| 40 | `FS_FORMAT`       | `a1` = blocks, `a2` = words/blk, `a3` = label         | 0 ok             | Format BRFS + sync (drives `/bin/format`) |

The syscall ABI allows a maximum of 3 arguments (`a1`–`a3` in `r5`–`r7`),
with the return value in `r1`. Where more data is needed, pointers are
passed. The `EXIT` syscall is special: it never returns to the caller —
instead it resets the hardware stack to the trampoline depth and jumps
directly to the BDOS return path, cleanly unwinding the entire user
program state.

**Network ownership:** The first call to `NET_SEND` or `NET_RECV` implicitly
takes ownership of the Ethernet controller away from the kernel's FNP
protocol handler. While a user program owns the network, the kernel will
not consume any incoming packets from the ring buffer. Ownership is
automatically released when the program exits, at which point the ring
buffer is also reset.

**Retired in Phase E:** the v3 syscalls `PRINT_CHAR` (0), `PRINT_STR` (1),
`READ_KEY` (2), `KEY_AVAILABLE` (3), `TERM_PUT_CELL` (16), `TERM_CLEAR`
(17), `TERM_SET_CURSOR` (18), `TERM_GET_CURSOR` (19), `SET_PALETTE` (22),
`SET_PIXEL_PALETTE` (26), `UART_PRINT_CHAR` (31), and `UART_PRINT_STR`
(32) were removed. Use `WRITE` on `fd 1` (with ANSI escapes for cursor /
color / clear), `WRITE` on `fd 2` for stderr / UART-mirrored debug output,
and `OPEN("/dev/tty", O_RDONLY|O_RAW[|O_NONBLOCK])` + `READ` for raw
keyboard event packets.

### User-side library

User programs link against the user syscall library (`Software/C/userlib/`),
which provides convenience wrappers around the raw syscall interface. Phase E
trimmed the wrapper set to match the trimmed syscall surface; see the
migration table at the top of `syscall.h` if you are porting older code.

```c
#include <syscall.h>

int main(void)
{
    sys_putstr("Hello from userBDOS!\n");      /* sys_write(1, ...) */
    sys_write(2, "boot trace\n", 11);          /* mirrored to UART  */
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

The convenience wrappers ultimately call the low-level `syscall()`
function, which contains the inline assembly that performs the jump to
address 12.

## Interrupt Handling

BDOS owns all hardware interrupts. The interrupt vector at address 4 points to BDOS's interrupt handler, which:

- Saves all registers to a dedicated interrupt stack (`0x3FFFFC`)
- Reads the interrupt ID via INTID
- Dispatches to the appropriate handler
- Restores registers and returns via `reti`

### Interrupt Dispatch

| INT ID | Source | Handler |
|--------|--------|---------|
| 1 | UART RX | *(no-op)* |
| 2 | Timer 1 | Deferred Ethernet ISR retry (when SPI is busy) |
| 3 | Timer 2 | USB keyboard polling (10 ms periodic) |
| 4 | Timer 3 | `delay()` completion |
| 5 | Frame Drawn | *(no-op)* |
| 6 | ENC28J60 RX | Ethernet packet reception into ring buffer |

The Ethernet interrupt (INT ID 6) is the primary network reception path. When a packet arrives, the ENC28J60 asserts its `~INT` pin, which triggers the ISR. The ISR drains all pending packets from the ENC28J60 into a kernel-managed ring buffer in SDRAM. If the SPI bus is busy (e.g., a packet is being transmitted), the ISR defers processing by starting a 1 ms one-shot timer on Timer 1 (INT ID 2), which retries the drain when it fires.

User programs do not receive interrupts directly, but in the future a syscall interface should allow them to register a callback for certain interrupts.

## Networking

BDOS uses interrupt-driven packet reception for Ethernet networking. The ENC28J60 Ethernet controller triggers a hardware interrupt (INT ID 6) when packets arrive. The ISR drains all pending packets from the ENC28J60's small 6 KiB hardware RX buffer into a 64-slot ring buffer in SDRAM, preventing packet loss during CPU-intensive operations.

Both the kernel FNP handler and user programs read from this ring buffer:

- **Kernel mode** (no user program owns the network): `bdos_fnp_poll()` reads packets from the ring buffer and processes FNP protocol messages (file transfers, keyboard input).
- **User mode** (a user program has called `NET_SEND` or `NET_RECV`): The kernel FNP poll is disabled, and the user program reads packets from the ring buffer via the `NET_RECV` syscall.

The SPI bus is shared between TX (user-initiated sends) and RX (ISR-driven reception). A mutex flag prevents the ISR from accessing SPI during a transmit operation; if the SPI is busy when an interrupt arrives, the ISR defers to a 1 ms timer that retries the drain.

See [FNP](FNP.md) for the network protocol details.

## Initialization

On boot, BDOS:

1. Initializes a bunch of hardware (GPU, timers, Ethernet, USB, SPI, etc)
2. Mounts BRFS from SPI flash into RAM cache. This takes some time depending on the size of the filesystem, so a progress bar is shown
3. Starts the interactive shell
