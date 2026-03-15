# OS (BDOS)

BDOS (Bart's Drive Operating System) is the custom operating system for the FPGC. It provides a shell, filesystem access, hardware drivers (Ethernet, USB/HID input, etc), and most importantly the ability to run user programs while providing system calls. It acts as a single program that is meant to be loaded from SPI flash by the bootloader on startup (depending on the boot switch), and allows the FPGC to be used as a standalone computer without needing to connect it to a host PC.

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

Divided into 8 slots of 2 MiB each. Programs (when compiled using the B32CC `-user-bdos` flag and assembled with ASMPY `-h -i`) are loaded into slots and execute with their stack at the top of their allocated slot. This should allow up to 8 concurrent user programs, although that would require a scheduler to manage them, which has yet to be implemented. Programs with lots of data should use the heap for dynamic allocation, and load data from BRFS into the heap at runtime.

| Slot | Address Range | Stack Top |
|------|---------------|-----------|
| 0 | `0x2000000` – `0x21FFFFF` | `0x21FFFFC` |
| 1 | `0x2200000` – `0x23FFFFF` | `0x23FFFFC` |
| 2 | `0x2400000` – `0x25FFFFF` | `0x25FFFFC` |
| ... | ... | ... |
| 7 | `0x2E00000` – `0x2FFFFFF` | `0x2FFFFFC` |

## Shell

BDOS provides a basic interactive shell with the following commands:

| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `clear` | Clear the terminal screen |
| `echo <text>` | Print text to the terminal |
| `uptime` | Show system uptime |
| `pwd` | Print working directory |
| `cd <path>` | Change directory |
| `ls [path]` | List directory contents |
| `cat <file>` | Display file contents |
| `write <file> <text>` | Write text to a file |
| `mkdir <path>` | Create a directory |
| `mkfile <path>` | Create an empty file |
| `rm <path>` | Remove a file or directory |
| `cp <src> <dest>` | Copy a file |
| `mv <src> <dest>` | Move/rename a file |
| `df` | Show filesystem usage |
| `sync` | Flush filesystem to flash |
| `format` | Format the filesystem (interactive wizard) |
| `jobs` | List running/suspended user programs |
| `fg <slot>` | Resume a suspended program |
| `kill <slot>` | Kill a running/suspended program |

Any non-built-in command is treated as a program name and resolved/executed automatically. Arguments are passed to the program via the `SHELL_ARGC` and `SHELL_ARGV` syscalls.

## Program Loading and Execution

Programs are launched by typing their name or path at the shell prompt. The dispatcher tries built-in commands first, then program resolution:

1. If the name contains `/` or starts with `.`: resolved as a path (absolute or relative to cwd)
2. Bare names: try `/bin/<name>` first, then fall back to cwd

Before execution, the shell stores `argc` and `argv` in kernel globals so the program can retrieve them via the `SHELL_ARGC` and `SHELL_ARGV` syscalls.

### Loading Process

1. **Path resolution**: If the name has no `/`, BDOS looks in `/bin/` automatically, then falls back to cwd
2. **Slot allocation**: A free slot is allocated via `bdos_slot_alloc()`
3. **Read binary**: The file is read from BRFS in 256-word chunks into the slot's memory region
4. **Cache flush**: The `ccache` instruction clears L1I/L1D caches to ensure the CPU fetches fresh instructions
5. **Register setup**:
      - `r13` (stack pointer) = top of slot
      - `r15` (return address) = BDOS return trampoline
6. **Jump**: Execution transfers to offset 0 of the slot, which contains the ASMPY header `jump Main`
7. **Return**: When the program's `main()` returns (via `r15`), BDOS restores its own registers, frees the slot (including heap cleanup), and prints the exit code

## Heap

The kernel heap region (`0x400000`–`0x1FFFFFF`, 28 MiB) is managed by a simple bump allocator. User programs allocate memory via the `HEAP_ALLOC` syscall. All allocations are freed together when the program exits, is killed, or encounters an error — there is no individual `free()`.

This design is intentional: the bump allocator is very fast and simple, and the per-program cleanup ensures no memory leaks across program invocations. Programs that need to "reallocate" a buffer simply allocate a new, larger block and copy the data over; the old block is wasted but freed on exit.

## Syscalls

User programs communicate with BDOS through a software syscall mechanism, as there is no dedicated trap or software interrupt instruction (I could not figure out how to make that work, as I do not want to run syscall code within an interrupt, blocking other interrupts from happening). Therefore, syscalls are implemented as regular jumps to a fixed kernel entry point.

### Mechanism

The ASMPY assembler header places a `jump Syscall` instruction at absolute address 12 (byte offset `0xC`, as part of the header, enabled with the `-s` flag). User programs invoke a syscall by jumping to this address with arguments in registers. The flow is:

1. User calls `syscall(num, a1, a2, a3)`, B32CC places arguments in `r4` to `r7`
2. The inline assembly saves `r15`, loads address 12 into a temp register, sets the return address via `savpc`/`add`, and jumps
3. Address 12 contains `jump Syscall`, redirecting into the kernel's assembly trampoline
4. The trampoline saves all registers (except `r1`) to the hardware stack, switches to the kernel syscall stack, and calls the C dispatcher
5. The C dispatcher handles the request, returns a result in `r1`
6. The trampoline restores registers and returns to the user program

### Available Syscalls

!!! note
    See `bdos_syscall.h` for the up to date list of syscall numbers.

| Number | Name | Arguments | Returns | Description |
|--------|------|-----------|---------|-------------|
| 0 | `PRINT_CHAR` | `a1` = character | 0 | Print a character to the terminal |
| 1 | `PRINT_STR` | `a1` = string pointer | 0 | Print a null-terminated string |
| 2 | `READ_KEY` | — | key code | Read a key from the keyboard buffer (-1 if empty) |
| 3 | `KEY_AVAILABLE` | — | 0 or 1 | Check if a key is available |
| 4 | `FS_OPEN` | `a1` = path pointer | file descriptor | Open a file |
| 5 | `FS_CLOSE` | `a1` = fd | 0 on success | Close a file |
| 6 | `FS_READ` | `a1` = fd, `a2` = buffer, `a3` = count | words read | Read from a file |
| 7 | `FS_WRITE` | `a1` = fd, `a2` = buffer, `a3` = count | words written | Write to a file |
| 8 | `FS_SEEK` | `a1` = fd, `a2` = offset | 0 on success | Seek to a position in a file |
| 9 | `FS_STAT` | `a1` = path, `a2` = entry_buf | 0 on success | Get file/directory metadata |
| 10 | `FS_DELETE` | `a1` = path | 0 on success | Delete a file or directory |
| 11 | `FS_CREATE` | `a1` = path | 0 on success | Create an empty file |
| 12 | `FS_FILESIZE` | `a1` = fd | file size in words | Get the size of an open file |
| 13 | `SHELL_ARGC` | — | argc | Get argument count for current program |
| 14 | `SHELL_ARGV` | — | pointer to argv[] | Get argument vector for current program |
| 15 | `SHELL_GETCWD` | — | pointer to cwd string | Get the shell's current working directory |
| 16 | `TERM_PUT_CELL` | `a1` = x, `a2` = y, `a3` = (tile<<8)\|palette | 0 | Write a tile+palette to a terminal cell |
| 17 | `TERM_CLEAR` | — | 0 | Clear the terminal screen |
| 18 | `TERM_SET_CURSOR` | `a1` = x, `a2` = y | 0 | Set the terminal cursor position |
| 19 | `TERM_GET_CURSOR` | — | (x<<8)\|y | Get the terminal cursor position (packed) |
| 20 | `HEAP_ALLOC` | `a1` = size_words | pointer (or 0) | Allocate memory from the kernel heap |
| 21 | `DELAY` | `a1` = milliseconds | 0 | Sleep for the given number of milliseconds |
| 22 | `SET_PALETTE` | `a1` = index, `a2` = value | 0 | Set a BGW tile palette entry (value = (bg<<8)\|fg, 8-bit RRRGGGBB) |
| 23 | `EXIT` | `a1` = exit code | *(does not return)* | Terminate the calling program immediately and return to BDOS |
| 24 | `FS_READDIR` | `a1` = path, `a2` = entry_buf, `a3` = index | 0 on success | Read a directory entry by index |
| 25 | `GET_KEY_STATE` | — | bitmap | Get the raw keyboard key-state bitmap |
| 26 | `SET_PIXEL_PALETTE` | `a1` = index (0–255), `a2` = 24-bit RGB | 0 | Set a pixel-plane palette color (0x00RRGGBB) |
| 27 | `NET_SEND` | `a1` = buffer, `a2` = length | 1 success, 0 error | Send a raw Ethernet frame (takes network ownership) |
| 28 | `NET_RECV` | `a1` = buffer, `a2` = max length | bytes received (0 if none) | Pop a packet from the RX ring buffer (non-blocking, takes network ownership) |
| 29 | `NET_PACKET_COUNT` | — | count | Number of packets in the RX ring buffer |
| 30 | `NET_GET_MAC` | `a1` = buffer (6 words) | 0 | Copy our 6-byte MAC address to the buffer |

The syscall ABI allows a maximum of 3 arguments (`a1`–`a3` in `r5`–`r7`), with the return value in `r1`. Where more data is needed, arguments are packed (e.g., `TERM_PUT_CELL` packs tile and palette into a single word) or pointers are used. The `EXIT` syscall is special: it never returns to the caller. Instead, it resets the hardware stack to the trampoline depth and jumps directly to the BDOS return path, cleanly unwinding the entire user program state.

**Network ownership:** The first call to `NET_SEND` or `NET_RECV` implicitly takes ownership of the Ethernet controller away from the kernel's FNP protocol handler. While a user program owns the network, the kernel will not consume any incoming packets from the ring buffer. Ownership is automatically released when the program exits (via `EXIT` or normal return), at which point the ring buffer is also reset.

### User-Side Library

User programs include the user syscall library via the user library orchestrator. Additional optional libraries (FNP networking, fixed-point math) are available via feature flags:

```c
#define USER_SYSCALL
#define USER_FNP       // Optional: FNP frame build/parse/reliable-send helpers
#define USER_FIXED64   // Optional: Q32.32 fixed-point math via FP64 coprocessor
#include "libs/user/user.h"

int main()
{
  sys_print_str("Hello from userBDOS!\n");
  return 0;
}
```

The convenience wrappers (`sys_print_char`, `sys_print_str`, etc.) call the low-level `syscall()` function, which contains the inline assembly that performs the jump to address 12.

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
