# BDOS Context (for AI coding tools)

BDOS is the kernel/OS for FPGC. It is a single-binary C program built from `Software/C/BDOS/main.c` using B32CC in `-bdos` mode.

## Build & Run

- `make compile-bdos` → compiles via `Scripts/BCC/compile_bdos.sh`
- `make run-bdos` → upload over UART
- `make flash-bdos` → flash to SPI

Pipeline: `main.c` → B32CC → `.asm` → ASMPY → `.list` → `.bin`

## Source Files

| File | Role |
|------|------|
| `bdos.h` | Shared defines, globals, library imports, function declarations |
| `mem_map.h` | Memory layout constants (must match `cgb32p3.inc` in B32CC) |
| `main.c` | Entry point, main loop, interrupt dispatcher, `bdos_panic()` |
| `init.c` | Hardware init: GPU, terminal, UART, timers, USB keyboard (CH376), Ethernet (ENC28J60) |
| `hid.c` | USB keyboard driver, HID report → key event translation, FIFO, key repeat |
| `fs.c` | BRFS mount/format/sync wrappers with progress bar rendering |
| `eth.c` | FNP (FPGC Network Protocol) over ENC28J60: file transfer, remote keycode injection |
| `syscall.c` | Syscall dispatcher (called from inline assembly trampoline in cgb32p3.inc) |
| `shell.c` | Interactive line editor with cursor movement, command history (ring buffer of 8) |
| `shell_cmds.c` | Built-in commands, format wizard, path resolution, user program loader |

All modules are `#include`d directly into `main.c` (single compilation unit, no linker).

## Library Imports

BDOS uses the orchestrator pattern: `bdos.h` defines flags before including orchestrator headers.

**Common** (`libs/common/common.h`): `COMMON_STRING`, `COMMON_STDLIB`, `COMMON_CTYPE`

**Kernel** (`libs/kernel/kernel.h`): `KERNEL_GPU_HAL`, `KERNEL_GPU_FB`, `KERNEL_GPU_DATA_ASCII`, `KERNEL_TERM`, `KERNEL_UART`, `KERNEL_SPI`, `KERNEL_SPI_FLASH`, `KERNEL_BRFS`, `KERNEL_TIMER`, `KERNEL_CH376`, `KERNEL_ENC28J60`

## Memory Map

| Region | Address Range | Size |
|--------|--------------|------|
| Kernel code+data | `0x000000`–`0x0FFFFF` | 1 MiW (4 MiB) |
| Kernel heap | `0x100000`–`0x7FFFFF` | 7 MiW (28 MiB) |
| User program slots | `0x800000`–`0xBFFFFF` | 4 MiW, 8 × 512 KiW slots |
| BRFS cache | `0xC00000`–`0xFFFFFF` | 4 MiW (16 MiB) |

Kernel stacks: main `0x0F7FFF`, syscall `0x0FBFFF`, interrupt `0x0FFFFF`.

## Boot Flow

1. `main()` → `bdos_init()` (GPU, terminal, UART, timers, Ethernet/FNP, USB keyboard)
2. `bdos_fs_boot_init()` → `brfs_init()` + `brfs_mount()`. On mount failure, sets `bdos_fs_boot_needs_format`.
3. `bdos_shell_init()` → clears screen, prints banner, calls `bdos_shell_on_startup()` (prompts format wizard if mount failed; declining → `bdos_panic()`).
4. `bdos_loop()` → forever: poll USB keyboard, poll FNP/Ethernet, run `bdos_shell_tick()`.

## Interrupt Model

`interrupt()` dispatches via `get_int_id()`:
- `INTID_TIMER1`: USB keyboard polling (10 ms periodic via `timer_set_callback`)
- `INTID_TIMER2`: `delay()` support

## Input/HID Subsystem

- USB host: CH376 via SPI (default bottom CH376)
- `bdos_usb_keyboard_main_loop()`: handles connect/disconnect/enumeration lifecycle (non-interrupt context)
- `bdos_poll_usb_keyboard()`: timer callback, reads HID reports, translates keycodes, pushes events to FIFO, handles key repeat (400 ms initial, 80 ms interval)
- Event FIFO: 64-entry ring buffer. Overflow drops new events with UART warning.
- Consumer API: `bdos_keyboard_event_available()`, `bdos_keyboard_event_read()` (returns `-1` when empty)

### Key Event Encoding

- Printable ASCII: raw value
- Ctrl+A..Z: control codes 1..26
- Special keys: `BDOS_KEY_*` constants (base `0x100`): arrows, Insert/Delete/Home/End/PageUp/PageDown, F1–F12

## Filesystem (BRFS)

- Flash target: `SPI_FLASH_1`
- RAM cache at `MEM_BRFS_START` (4 MiW)
- Flash sync is explicit only (`sync` command or after format)
- Progress callbacks rendered as terminal progress bars during mount/format/sync

## Networking (FNP)

Custom Layer 2 protocol over ENC28J60 Ethernet (EtherType `0xB4B4`).

- MAC derived from SPI Flash 0 unique ID (prefix `02:B4:B4:00:00:XX`)
- Message types: `FILE_START`/`FILE_DATA`/`FILE_END`/`FILE_ABORT` (reliable file transfer with ACK/NACK and checksum), `KEYCODE` (remote keyboard input injection), `MESSAGE`
- `bdos_fnp_poll()` is called each main loop iteration (non-blocking)

## Syscall Interface

User programs invoke syscalls via an assembly trampoline. Dispatcher in `syscall.c`:

| Number | Name | Args |
|--------|------|------|
| 0 | `PRINT_CHAR` | char |
| 1 | `PRINT_STR` | char* |
| 2 | `READ_KEY` | — (returns key event) |
| 3 | `KEY_AVAILABLE` | — (returns count) |
| 4 | `FS_OPEN` | path |
| 5 | `FS_CLOSE` | fd |
| 6 | `FS_READ` | fd, buf, count |
| 7 | `FS_WRITE` | fd, buf, count |

## User Program Execution

Programs are launched by typing their name or path at the shell prompt (like Linux). The dispatcher tries built-in commands first, then program resolution:

1. If name contains `/` or starts with `.`: resolved as a path (absolute or relative to cwd)
2. Bare names: try `/bin/<name>` first, then fall back to cwd

`bdos_exec_program(path)` handles loading: allocates a slot via `bdos_slot_alloc()`, reads the binary from BRFS into the slot's memory region, flushes icache, runs it via inline assembly trampoline, restores BDOS state on return, then frees the slot.

## Multitasking

BDOS supports preemptive multitasking with up to 8 user programs in separate memory slots. Programs can be suspended, resumed, and killed via hotkeys, with no program cooperation required.

### CPU I/O Registers

Two memory-mapped I/O registers (handled inside `B32P3.v`, not through MemoryUnit):

| Register | Address | Description |
|----------|---------|-------------|
| `IO_PC_BACKUP` | `0x7C00000` | Interrupt return PC (read/write) |
| `IO_HW_STACK_PTR` | `0x7C00001` | Hardware stack pointer 0–255 (read/write) |

Hardware stack: 256 entries (increased from 128 for multitasking).

### Slot State

Per-slot parallel arrays: `bdos_slot_status[]` (EMPTY/RUNNING/SUSPENDED), `bdos_slot_name[]`, saved registers/HW stack. Only one slot can be RUNNING at a time. BDOS shell runs when `bdos_active_slot == BDOS_SLOT_NONE`.

### Hotkeys (during program execution)

- `F1`–`F8`: suspend currently running program and switch to slot (F1=slot 0, F2=slot 1, ...)
- `F12`: suspend currently running program and return to BDOS
- `Alt F4`: kill currently running program and return to BDOS

### Suspend Flow

1. Keyboard polling (in interrupt handler) detects F12 → sets `bdos_switch_target`
2. `interrupt()` writes `IO_PC_BACKUP = bdos_save_and_switch`
3. Normal `Return_Interrupt` pops registers, `reti` → enters `bdos_save_and_switch`
4. Saves user registers to temp array, switches to BDOS stack
5. Pops user HW stack entries (excluding trampoline), saves to slot state
6. Marks slot SUSPENDED, returns to `bdos_loop()`

### Resume Flow (`fg` command)

1. `bdos_resume_program(slot)` pushes fresh trampoline entries to HW stack
2. Pushes saved user HW stack entries (reverse order) and interrupt registers
3. Sets `IO_PC_BACKUP = saved_pc`, jumps to `Return_Interrupt`
4. `reti` resumes user program exactly where suspended
5. When user program eventually exits normally, trampoline return restores BDOS state

## Shell

### Built-in Commands

`help`, `clear`, `echo`, `uptime`, `pwd`, `cd`, `ls`, `df`, `mkdir`, `mkfile`, `rm`, `cat`, `write`, `format`, `sync`, `jobs`, `fg <slot>`, `kill <slot>`

Any non-built-in command is treated as a program name and resolved/executed automatically.

### Line Editor

- Cursor movement (left/right arrows), insert anywhere, backspace/delete
- Command history: 8-entry ring buffer, navigated with up/down arrows
- Ctrl+C clears input, Ctrl+L clears screen
- Visual cursor rendered by palette inversion

### Special Input Modes

The shell has modal input for the format wizard (multi-step: blocks → words/block → label → full format y/n) and boot-time format confirmation.

## Coding Guidelines

- Keep C simple: no complex macros, no struct returns, no advanced constructs (B32CC limitations).
- Preserve single-compilation-unit pattern (`main.c` includes all modules).
- `TIMER_1` is taken by HID polling; `TIMER_2` by `delay()`. Use `TIMER_0` if a new timer is needed.
- Keep FIFO API stable — shell and FNP both push events into it.
- New shell commands: add handler function in `shell_cmds.c`, add dispatch entry in `bdos_shell_execute_line()`.
- New syscalls: add define in `bdos.h`, add case in `bdos_syscall_dispatch()`.
