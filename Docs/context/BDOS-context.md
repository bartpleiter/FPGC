# BDOS Context (for AI coding tools)

BDOS is the kernel/OS for FPGC. It is built from multiple separately-compiled C and assembly source files using the modern toolchain (cproc + QBE + ASMPY linker).

## Build & Run

- `make compile-bdos` → compiles all BDOS sources via `Scripts/BCC/compile_modern_c.sh`
- `make run-bdos` → upload over UART
- `make flash-bdos` → flash to SPI

Pipeline: `crt0_bdos.asm + libc sources + libfpgc sources + bdos sources → cproc → QBE → asm → linker → ASMPY → .list → .bin`

## Source Files

BDOS is organized as three libraries plus the kernel:

### libc (`Software/C/libc/`)

Standard C library (picolibc-derived). Provides `string.h`, `stdlib.h`, `stdio.h`, `ctype.h`, `stdint.h`, etc. System stubs in `sys/syscalls.c` (UART I/O), `sys/hwio.asm` (memory-mapped I/O helpers), `sys/_exit.asm`.

### libfpgc (`Software/C/libfpgc/`)

Hardware abstraction layer. Headers in `include/`:

| Header | Purpose |
|--------|---------|
| `fpgc.h` | Memory map constants, I/O addresses, `hwio_write()`/`hwio_read()` |
| `sys.h` | `get_int_id()`, `get_boot_mode()`, `set_user_led()`, `get_micros()` |
| `timer.h` | Timer management (3 hardware timers) |
| `uart.h` | UART I/O |
| `spi.h` | SPI bus driver |
| `spi_flash.h` | SPI Flash driver |
| `ch376.h` | CH376 USB host driver |
| `enc28j60.h` | ENC28J60 Ethernet driver |
| `gpu_hal.h` | GPU hardware abstraction |
| `gpu_fb.h` | Framebuffer operations |
| `gpu_data_ascii.h` | ASCII font/palette data |
| `term.h` | Terminal emulation (25-line display + 200-line scrollback) |
| `brfs.h` | BRFS filesystem interface |
| `debug.h` | Debugging utilities |

### BDOS kernel (`Software/C/bdos/`)

| File | Role |
|------|------|
| `include/bdos.h` | Master include — pulls in libc, libfpgc, all bdos_* headers |
| `include/bdos_syscall.h` | Syscall number definitions (0–30) |
| `include/bdos_mem_map.h` | Memory layout constants |
| `include/bdos_heap.h` | Heap allocator interface |
| `include/bdos_slot.h` | Program slot management interface |
| `include/bdos_hid.h` | HID/keyboard subsystem, key state bitmap constants |
| `include/bdos_fs.h` | Filesystem integration interface |
| `include/bdos_fnp.h` | FNP protocol definitions |
| `include/bdos_shell.h` | Shell interface, argc/argv globals |
| `main.c` | Entry point, interrupt handler, main loop |
| `init.c` | Hardware initialization |
| `syscall.c` | Syscall dispatcher |
| `heap.c` | Bump allocator for kernel heap |
| `slot.c` | Program slot management (load, run, suspend, resume, kill) |
| `slot_asm.asm` | Assembly helpers for context switching and program execution |
| `hid.c` | USB keyboard driver, HID report translation, FIFO, key state bitmap |
| `fs.c` | BRFS mount/format/sync wrappers with progress bars |
| `eth.c` | FNP Ethernet protocol: file transfer, remote keycode injection |
| `shell.c` | Interactive line editor with cursor movement, command history |
| `shell_cmds.c` | Built-in commands, program loader |
| `shell_path.c` | Path resolution |
| `shell_util.c` | Shell utility functions |
| `shell_format.c` | Format wizard |

Each `.c` file is compiled independently by cproc and linked together by the assembly-level linker. No orchestrator pattern — standard `#include <header.h>` with `-I` paths.

## Memory Map (byte addresses)

| Region | Address Range | Size |
|--------|--------------|------|
| Kernel code+data | `0x000000`–`0x3FFFFF` | 4 MiB |
| Kernel heap | `0x400000`–`0x1FFFFFF` | 28 MiB |
| User program slots | `0x2000000`–`0x2FFFFFF` | 16 MiB, 8 × 2 MiB slots |
| BRFS cache | `0x3000000`–`0x3FFFFFF` | 16 MiB |

Kernel stacks: main `0x3DFFFC`, syscall `0x3EFFFC`, interrupt `0x3FFFFC`.

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
- `bdos_poll_usb_keyboard()`: timer callback, reads HID reports, translates keycodes, pushes events to FIFO, handles key repeat (400 ms initial, 80 ms interval), updates key state bitmap
- Event FIFO: 64-entry ring buffer. Overflow drops new events with UART warning.
- Consumer API: `bdos_keyboard_event_available()`, `bdos_keyboard_event_read()` (returns `-1` when empty)

### Key Event Encoding

- Printable ASCII: raw value
- Ctrl+A..Z: control codes 1..26
- Special keys: `BDOS_KEY_*` constants (base `0x100`): arrows, Insert/Delete/Home/End/PageUp/PageDown, F1–F12
- Note: Ctrl+Up/Down are filtered from the FIFO (handled by terminal scrollback via key state bitmap)

### Key State Bitmap

A global `bdos_key_state_bitmap` (updated every HID poll cycle from the raw USB keyboard report) provides real-time "key is held" state for all commonly-used game/navigation keys. Unlike the event FIFO, the bitmap reflects simultaneous key state and has no repeat delay.

| Bit | Constant | Key |
|-----|----------|-----|
| 0x0001 | `KEYSTATE_W` | W |
| 0x0002 | `KEYSTATE_A` | A |
| 0x0004 | `KEYSTATE_S` | S |
| 0x0008 | `KEYSTATE_D` | D |
| 0x0010 | `KEYSTATE_UP` | Up Arrow |
| 0x0020 | `KEYSTATE_DOWN` | Down Arrow |
| 0x0040 | `KEYSTATE_LEFT` | Left Arrow |
| 0x0080 | `KEYSTATE_RIGHT` | Right Arrow |
| 0x0100 | `KEYSTATE_SPACE` | Space |
| 0x0200 | `KEYSTATE_SHIFT` | Shift |
| 0x0400 | `KEYSTATE_CTRL` | Ctrl |
| 0x0800 | `KEYSTATE_ESCAPE` | Escape |
| 0x1000 | `KEYSTATE_E` | E |
| 0x2000 | `KEYSTATE_Q` | Q |

`bdos_rebuild_key_state_bitmap()` is called in the timer ISR after each HID report read. The bitmap is also accessible to user programs via syscall 25 (`GET_KEY_STATE`).

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

## Heap

A bump allocator manages the kernel heap region (`0x100000`–`0x7FFFFF`, 7 MiW). User programs allocate via the `HEAP_ALLOC` syscall. All allocations are freed together when the program exits (in `bdos_slot_free()`). There is no individual `free()` — programs that need reallocation simply allocate a new block and abandon the old one.

- `bdos_heap_init()` — called during boot, resets the bump pointer to `MEM_HEAP_START`
- `bdos_heap_alloc(size_words)` — returns pointer to allocated block, or 0 on failure
- `bdos_heap_free_all()` — resets the bump pointer (called on program exit/kill)

## Syscall Interface

User programs invoke syscalls via an assembly trampoline. ABI: `r4` = syscall number, `r5`–`r7` = up to 3 arguments, return value in `r1`. Dispatcher in `syscall.c`:

| Number | Name | Args | Returns |
|--------|------|------|---------|
| 0 | `PRINT_CHAR` | char | 0 |
| 1 | `PRINT_STR` | char* | 0 |
| 2 | `READ_KEY` | — | key event (or -1) |
| 3 | `KEY_AVAILABLE` | — | 0 or 1 |
| 4 | `FS_OPEN` | path | fd |
| 5 | `FS_CLOSE` | fd | 0 on success |
| 6 | `FS_READ` | fd, buf, count | bytes read |
| 7 | `FS_WRITE` | fd, buf, count | bytes written |
| 8 | `FS_SEEK` | fd, offset | 0 on success |
| 9 | `FS_STAT` | path, entry_buf | 0 on success |
| 10 | `FS_DELETE` | path | 0 on success |
| 11 | `FS_CREATE` | path | 0 on success |
| 12 | `FS_FILESIZE` | fd | size in bytes |
| 13 | `SHELL_ARGC` | — | argc |
| 14 | `SHELL_ARGV` | — | pointer to argv[] |
| 15 | `SHELL_GETCWD` | — | pointer to cwd string |
| 16 | `TERM_PUT_CELL` | x, y, (tile<<8)\|palette | 0 |
| 17 | `TERM_CLEAR` | — | 0 |
| 18 | `TERM_SET_CURSOR` | x, y | 0 |
| 19 | `TERM_GET_CURSOR` | — | (x<<8)\|y |
| 20 | `HEAP_ALLOC` | size_bytes | pointer (or 0) |
| 21 | `DELAY` | milliseconds | 0 |
| 22 | `SET_PALETTE` | index, value | 0 |
| 23 | `EXIT` | exit_code | *(does not return)* |
| 24 | `FS_READDIR` | path, entry_buf, max | entry count (or <0 error) |
| 25 | `GET_KEY_STATE` | — | key state bitmap |
| 26 | `SET_PIXEL_PALETTE` | index, rgb24 | 0 |
| 27 | `NET_SEND` | buf, len | bytes sent |
| 28 | `NET_RECV` | buf, max_len | bytes received |
| 29 | `NET_PACKET_COUNT` | — | count |
| 30 | `NET_GET_MAC` | mac_buf | 0 |

Note: `TERM_PUT_CELL` packs tile and palette into a single argument (`a3`) because the syscall ABI only allows 3 arguments. `TERM_GET_CURSOR` packs x and y into the return value similarly. `SET_PALETTE` writes to `GPU_PALETTE_TABLE_ADDR + index`; value format is `(bg_color << 8) | fg_color` with 8-bit RRRGGGBB colors. `EXIT` terminates the calling program immediately by resetting the HW stack to the trampoline depth and jumping to the BDOS return path — the exit code is reported as the program's return value. `FS_READDIR` fills `entry_buf` with raw `brfs_dir_entry` structs and returns the number of entries. `GET_KEY_STATE` returns the current key state bitmap — see Key State Bitmap section above. `SET_PIXEL_PALETTE` sets a 320×240 pixel framebuffer palette entry (RGB24). `NET_SEND`/`NET_RECV`/`NET_PACKET_COUNT`/`NET_GET_MAC` provide raw Ethernet frame I/O for user programs (used by FNP protocol library).

## User Program Execution

Programs are launched by typing their name or path at the shell prompt (like Linux). The dispatcher tries built-in commands first, then program resolution:

1. If name contains `/` or starts with `.`: resolved as a path (absolute or relative to cwd)
2. Bare names: try `/bin/<name>` first, then fall back to cwd

`bdos_exec_program(path)` handles loading: allocates a slot via `bdos_slot_alloc()`, reads the binary from BRFS into the slot's memory region, flushes icache, runs it via assembly trampoline (in `slot_asm.asm`), restores BDOS state on return, then frees the slot.

## Multitasking

BDOS supports preemptive multitasking with up to 8 user programs in separate memory slots (2 MiB each). Programs can be suspended, resumed, and killed via hotkeys, with no program cooperation required.

### CPU I/O Registers

Two memory-mapped I/O registers (handled inside `B32P3.v`, not through MemoryUnit):

| Register | Address | Description |
|----------|---------|-------------|
| `IO_PC_BACKUP` | `0x1F000000` | Interrupt return PC (read/write) |
| `IO_HW_STACK_PTR` | `0x1F000004` | Hardware stack pointer 0–255 (read/write) |

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

`help`, `clear`, `echo`, `uptime`, `pwd`, `cd`, `ls`, `df`, `mkdir`, `mkfile`, `rm`, `cat`, `write`, `cp`, `mv`, `format`, `sync`, `jobs`, `fg <slot>`, `kill <slot>`

Any non-built-in command is treated as a program name and resolved/executed automatically.

### Line Editor

- Cursor movement (left/right arrows), insert anywhere, backspace/delete
- Command history: 8-entry ring buffer, navigated with up/down arrows
- Ctrl+C clears input, Ctrl+L clears screen
- Ctrl+Up/Down: scroll back/forward through terminal history (key-held via bitmap, ~33 lines/sec)
- Visual cursor rendered by palette inversion

### Terminal Scrollback

The terminal maintains a 200-line ring buffer (`history_tiles`/`history_palettes` in `term.c`) of lines that have scrolled off the top of the 25-line display. This enables reviewing output that has scrolled past.

- **Ctrl+Up**: scroll view back into history (per-line, rate-limited to 30 ms)
- **Ctrl+Down**: scroll view forward towards current output
- **Any new output** (keypress, command output): auto-snaps back to live view
- **`clear` command**: resets the scrollback history buffer
- Implementation: `bdos_shell_tick()` checks `bdos_key_state_bitmap` for Ctrl+arrow state each main loop iteration. Ctrl+Up/Down are filtered from the event FIFO so they don't interfere with shell history navigation.
- GPU refresh: `term_scroll_view_refresh()` redraws all 25 rows from the appropriate mix of history and screen buffer data.

### Special Input Modes

The shell has modal input for the format wizard (multi-step: blocks → words/block → label → full format y/n) and boot-time format confirmation.

## Coding Guidelines

- BDOS is built with the modern C toolchain (cproc + QBE). Full C11 is available.
- Each source file is compiled independently — use `#include <header.h>` with proper `-I` paths.
- No inline assembly. Assembly code goes in dedicated `.asm` files (e.g., `slot_asm.asm`, `sys_asm.asm`).
- All MMIO access goes through `hwio_write()`/`hwio_read()` from `fpgc.h` (cproc has no `volatile`).
- `TIMER_1` is taken by HID polling; `TIMER_2` by `delay()`. Use `TIMER_0` if a new timer is needed.
- Keep FIFO API stable — shell and FNP both push events into it.
- New shell commands: add handler function in `shell_cmds.c`, add dispatch entry in `bdos_shell_execute_line()`.
- New syscalls: add define in `bdos_syscall.h`, add case in `bdos_syscall_dispatch()` in `syscall.c`.

## User Programs

User programs live in `Software/C/userBDOS/` and are compiled with `make compile-userbdos file=<name>`. They use `crt0_userbdos.asm` as startup code and the `userlib` library for syscalls and utilities.

Compilation: `make compile-userbdos file=snake` → `crt0_userbdos.asm + userlib + snake.c → cproc → QBE → asm → linker → ASMPY → bin`

Legacy B32CC user programs are archived in `Software/C/b32cc/userBDOS/` and are being ported to the modern toolchain.

Notable programs:

| Program | Description |
|---------|-------------|
| `edit` | Text editor with gap buffer, colored status bars, horizontal/vertical scrolling, file save/load |
| `cmatrix` | Matrix rain effect — green-on-black palette, LFSR RNG, exits on Escape/Q |
| `snake` | Snake game — event-based input (arrow/WASD), adjustable speed (+/=), LFSR RNG, exits on Escape/Q |
| `tree` | Directory tree listing — recursive, box-drawing characters, optional path argument, summary line |
| `w3d` | Wolfenstein 3D raycaster — Q16.16 fixed-point, texture-mapped walls |
| `mbrot` | Mandelbrot set viewer — FP64 coprocessor, pixel framebuffer, interactive zoom |
| `bench` | FPGCbench — benchmark suite with microsecond timing |
