# OS (BDOS)

BDOS (Bart's Drive Operating System) is the custom operating system for the FPGC. It provides a shell, filesystem access, hardware drivers (Ethernet, USB/HID input, etc), and most importantly the ability to run user programs while providing system calls. It acts as a single program that is meant to be loaded from SPI flash by the bootloader on startup (depending on the boot switch), and allows the FPGC to be used as a standalone computer without needing to connect it to a host PC.

!!! note
    In this documentation and throughout the codebase, BDOS is also sometimes referred to as the kernel.

## Memory Layout

BDOS organizes the FPGC's 16 MiW (64 MiB) SDRAM into four regions:

| Address Range | Size | Description |
|---------------|------|-------------|
| `0x000000` – `0x0FFFFF` | 1 MiW (4 MiB) | Kernel code, data, and stacks |
| `0x100000` – `0x7FFFFF` | 7 MiW (28 MiB) | Kernel heap (dynamic allocation) |
| `0x800000` – `0xBFFFFF` | 4 MiW (16 MiB) | User program slots |
| `0xC00000` – `0xFFFFFF` | 4 MiW (16 MiB) | BRFS filesystem cache |

### Kernel Region (0x000000)

Contains the BDOS binary (code + data). Three stacks grow downward from the top:

- **Main stack**: top at `0x0F7FFF`
- **Syscall stack**: top at `0x0FBFFF`
- **Interrupt stack**: top at `0x0FFFFF`

### User Program Region (0x800000)

Divided into 8 slots of 512 KiW (2 MiB) each. Programs (when compiled using the B32CC `-user-bdos` flag and assembled with ASMPY `-h -i`) are loaded into slots and execute with their stack at the top of their allocated slot. This should allow up to 8 concurrent user programs, although that would require a scheduler to manage them, which has yet to be implemented. Programs with lots of data should use the heap for dynamic allocation, and load data from BRFS into the heap at runtime.

| Slot | Address Range | Stack Top |
|------|---------------|-----------|
| 0 | `0x800000` – `0x87FFFF` | `0x87FFFF` |
| 1 | `0x880000` – `0x8FFFFF` | `0x8FFFFF` |
| 2 | `0x900000` – `0x97FFFF` | `0x97FFFF` |
| ... | ... | ... |
| 7 | `0xB80000` – `0xBFFFFF` | `0xBFFFFF` |

## Shell

BDOS provides a basic interactive shell with the following commands:

| Command | Description |
|---------|-------------|
| `help` | List available commands |
| `clear` | Clear the terminal screen |
| `echo <text>` | Print text to the terminal |
| `uptime` | Show system uptime |
| `run <program>` | Load and execute a user program |
| `pwd` | Print working directory |
| `cd <path>` | Change directory |
| `ls [path]` | List directory contents |
| `cat <file>` | Display file contents |
| `write <file> <text>` | Write text to a file (for testing, should be replaced by a text editor program) |
| `mkdir <path>` | Create a directory |
| `mkfile <path>` | Create an empty file |
| `rm <path>` | Remove a file or directory |
| `df` | Show filesystem usage |
| `sync` | Flush filesystem to flash |
| `format` | Format the filesystem (interactive wizard) |

!!! note
    No piping or output redirection have been implemented yet, so the `echo` and `pwd` commands are not that useful currently.

## Program Loading and Execution

The `run` command loads a binary from BRFS into a user program slot and transfers control to it.

!!! note
    In the (near) future, this command should be removed and programs should be able to be loaded by just typing their name or path in the terminal, with argument support, and a program manager that keeps track of which programs are loaded in which slots. `run` is for now a temporary way to test user programs and allow development of things like syscalls.

### Loading Process

1. **Path resolution**: If the argument has no `/`, BDOS looks in `/bin/` automatically
2. **Read binary**: The file is read from BRFS in 256-word chunks into slot 0 (`0x800000`)
3. **Cache flush**: The `ccache` instruction clears L1I/L1D caches to ensure the CPU fetches fresh instructions
4. **Register setup**:
      - `r13` (stack pointer) = top of slot (`0x87FFFF`)
      - `r15` (return address) = BDOS return trampoline
5. **Jump**: Execution transfers to offset 0 of the slot, which contains the ASMPY header `jump Main`
6. **Return**: When the program's `main()` returns (via `r15`), BDOS restores its own registers and prints the exit code

## Syscalls

User programs communicate with BDOS through a software syscall mechanism, as there is no dedicated trap or software interrupt instruction (I could not figure out how to make that work, as I do not want to run syscall code within an interrupt, blocking other interrupts from happening). Therefore, syscalls are implemented as regular jumps to a fixed kernel entry point.

### Mechanism

The ASMPY assembler header places a `jump Syscall` instruction at absolute address 3 (as part of the header, enabled with the `-s` flag). User programs invoke a syscall by jumping to this address with arguments in registers. The flow is:

1. User calls `syscall(num, a1, a2, a3)`, B32CC places arguments in `r4` to `r7`
2. The inline assembly saves `r15`, loads address 3 into a temp register, sets the return address via `savpc`/`add`, and jumps
3. Address 3 contains `jump Syscall`, redirecting into the kernel's assembly trampoline
4. The trampoline saves all registers (except `r1`) to the hardware stack, switches to the kernel syscall stack, and calls the C dispatcher
5. The C dispatcher handles the request, returns a result in `r1`
6. The trampoline restores registers and returns to the user program

### Available Syscalls

!!! note
    This list will likely grow over time as I am extending BDOS. See `bdos.h` for the currently implemented list of syscalls.

| Number | Name | Arguments | Returns | Description |
|--------|------|-----------|---------|-------------|
| 0 | `PRINT_CHAR` | `a1` = character | 0 | Print a character to the terminal |
| 1 | `PRINT_STR` | `a1` = string pointer | 0 | Print a null-terminated string |
| 2 | `READ_KEY` | — | key code | Read a key from the keyboard buffer |
| 3 | `KEY_AVAILABLE` | — | 0 or 1 | Check if a key is available |
| 4 | `FS_OPEN` | `a1` = path pointer | file descriptor | Open a file |
| 5 | `FS_CLOSE` | `a1` = fd | 0 on success | Close a file |
| 6 | `FS_READ` | `a1` = fd, `a2` = buffer, `a3` = count | bytes read | Read from a file |
| 7 | `FS_WRITE` | `a1` = fd, `a2` = buffer, `a3` = count | bytes written | Write to a file |

### User-Side Library

User programs include the user syscall library via the user library orchestrator:

```c
#define USER_SYSCALL
#include "libs/user/user.h"

int main()
{
  sys_print_str("Hello from userBDOS!\n");
  return 0;
}
```

The convenience wrappers (`sys_print_char`, `sys_print_str`, etc.) call the low-level `syscall()` function, which contains the inline assembly that performs the jump to address 3.

## Interrupt Handling

BDOS owns all hardware interrupts. The interrupt vector at address 1 points to BDOS's interrupt handler, which:

- Saves all registers to a dedicated interrupt stack (`0x0FFFFF`)
- Reads the interrupt ID
- Dispatches to the appropriate handler (USB keyboard polling on Timer 1, delay on Timer 2)
- Restores registers and returns via `reti`

User programs do not receive interrupts directly, but in the future a syscall interface should allow them to register a callback for certain interrupts.

## Networking

BDOS includes an FNP (FPGC Network Protocol) handler that processes incoming Ethernet frames in the main loop. See [FNP](FNP.md) for more details.

## Initialization

On boot, BDOS:

1. Initializes a bunch of hardware (GPU, timers, Ethernet, USB, SPI, etc)
2. Mounts BRFS from SPI flash into RAM cache. This takes some time depending on the size of the filesystem, so a progress bar is shown
3. Starts the interactive shell
