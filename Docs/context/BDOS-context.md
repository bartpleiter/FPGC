# BDOS Context (for AI coding tools)

BDOS is the kernel/OS for FPGC. Built from many separately-compiled C
+ assembly source files using the modern toolchain
(cproc → QBE → ASMPY linker → flat binary). For the project-wide
overview see [Project-context.md](Project-context.md); for end-user
docs see [Docs/docs/Software/](../docs/Software/).

## Build & run

- `make compile-bdos`     — compile all BDOS sources (~270 KB binary)
- `make run-bdos`         — upload over UART
- `make flash-bdos`       — flash to SPI

Pipeline: `crt0_bdos.asm + libc + libfpgc + bdos sources → cproc → QBE
→ asm → ASMPY linker → .list → .bin`.

## Source layout

### libc (`Software/C/libc/`)

picolibc-derived freestanding C library: `string.h`, `stdlib.h`,
`stdio.h` (printf family), `ctype.h`, `stdint.h`, `errno.h`, ...
System hooks live in `sys/`. printf goes through `_write` which the
BDOS build wires to libterm v2.

### libfpgc (`Software/C/libfpgc/`)

Hardware abstraction. Headers in `include/`:

| Header | Purpose |
|--------|---------|
| `fpgc.h`           | Memory map constants, MMIO addresses |
| `sys.h`            | `get_int_id()`, `get_boot_mode()`, `set_user_led()`, `get_micros()` |
| `timer.h`          | 3 hardware timers + callbacks |
| `uart.h`           | UART I/O (TX + RX ring) |
| `spi.h`, `spi_flash.h` | SPI bus + SPI-flash driver |
| `ch376.h`          | CH376 USB-host driver |
| `enc28j60.h`       | ENC28J60 Ethernet driver |
| `gpu_hal.h`, `gpu_fb.h`, `gpu_data_ascii.h` | GPU + framebuffer + ASCII assets |
| **`term2.h`**      | libterm v2 — owns terminal cell grid, ANSI parser, scrollback, alt screen, line discipline |
| `brfs.h`, `brfs_storage.h` | BRFS v2 + storage backend vtable |
| `debug.h`          | Hex-dump helpers |

Note: the v1 `term.h` shim was deleted in shell-terminal-v2 Phase E.
All callers now use `term2_*` directly.

### BDOS kernel (`Software/C/bdos/`)

| File | Role |
|------|------|
| `include/bdos.h`         | Master include — pulls libc, libfpgc, all bdos_* headers |
| `include/bdos_syscall.h` | Syscall numbers (kept set + reserved/retired) |
| `include/bdos_mem_map.h` | Memory layout constants |
| `include/bdos_heap.h`    | Heap allocator interface |
| `include/bdos_slot.h`    | Slot management (loader / runner) |
| `include/bdos_proc.h`    | PID table + per-process FD table |
| `include/bdos_vfs.h`     | Byte-oriented VFS API |
| `include/bdos_hid.h`     | USB-keyboard subsystem + key-state bitmap |
| `include/bdos_fs.h`      | BRFS mount/format/sync wrappers |
| `include/bdos_fnp.h`     | FNP protocol definitions |
| `include/bdos_shell.h`   | Shell entry points + argc/argv globals |
| `main.c`                 | Entry point, interrupt handler, main loop |
| `init.c`                 | Hardware initialisation |
| `syscall.c`              | Syscall C dispatcher (single switch) |
| `heap.c`                 | Bump allocator for kernel heap |
| `slot.c` + `slot_asm.asm`| Program-slot loader, context switch, exec/return |
| `proc.c`                 | PID table + per-process state |
| `vfs.c`                  | File/tty/null/pipe device table; per-process fds |
| `hid.c`                  | USB keyboard driver, HID translation, FIFO, key state |
| `fs.c`                   | BRFS mount/format/sync with progress bars |
| `eth.c`                  | FNP file-transfer + remote-keycode injection |
| `shell.c`                | Line editor, prompt, command dispatch |
| `shell_lex.c`            | Tokenizer (quoting, operators, escapes) |
| `shell_parse.c`          | AST builder (commands, pipelines, chains, redirs) |
| `shell_exec.c`           | Built-in registry + program launcher; pipes via temp files |
| `shell_path.c`           | `/bin/<name>` then cwd lookup |
| `shell_script.c`         | `#!/bin/sh` interpreter (`$0`–`$9`, `$#`, `$?`, `set -e`) |
| `shell_vars.c`           | Shell + environment variables |
| `shell_cmds.c`           | Built-in implementations (`bi_*`) |
| `shell_format.c`         | Boot-time mount-failure format wizard |
| `shell_util.c`           | Misc shell helpers |

Each `.c` is compiled independently; standard
`#include <header.h>` with `-I` paths set by the build script.

## Memory map (byte addresses)

| Region | Range | Size |
|--------|-------|------|
| Kernel code + data | `0x000000`–`0x3FFFFF` | 4 MiB |
| Kernel heap | `0x400000`–`0x1FFFFFF` | 28 MiB |
| User program slots | `0x2000000`–`0x2FFFFFF` | 16 MiB (8 × 2 MiB) |
| BRFS RAM cache | `0x3000000`–`0x3FFFFFF` | 16 MiB |

Kernel stacks: main `0x3DFFFC`, syscall `0x3EFFFC`, interrupt
`0x3FFFFC`. The hardware stack is 256 entries (raised from 128 to
support multi-slot job control).

## Boot flow

1. `main()` → `bdos_init()` — GPU + libterm v2, UART, timers, SPI,
   USB keyboard (CH376), Ethernet (ENC28J60).
2. `bdos_fs_boot_init()` — `brfs_init()` then `brfs_mount()`. On
   mount failure sets `bdos_fs_boot_needs_format`.
3. `bdos_shell_init()` — banner; if mount failed, run the
   in-kernel format wizard from `shell_format.c`.
4. `bdos_loop()` — forever: poll keyboard, poll FNP/Ethernet, run
   `bdos_shell_tick()`.

## Interrupts

`interrupt()` reads `INTID` and dispatches:

| INT ID | Source | Handler |
|--------|--------|---------|
| 1 | UART RX | (no-op) |
| 2 | Timer 1 | Deferred ENC28J60 ISR retry (SPI was busy) |
| 3 | Timer 2 | USB keyboard polling (10 ms periodic) |
| 4 | Timer 3 | `delay()` completion |
| 5 | Frame Drawn | (no-op) |
| 6 | ENC28J60 RX | Drain hardware RX into 64-slot kernel ring |

## Input / HID

USB keyboard via CH376 over SPI. HID polling lives in the timer ISR.

- Event FIFO: 64-entry ring; overflow drops new events with a UART
  warning. Consumers: `bdos_keyboard_event_available()` /
  `bdos_keyboard_event_read()`.
- Key encoding (in the FIFO and in the 4-byte raw `/dev/tty` packets):
  - Printable ASCII: raw value
  - Ctrl+A..Z: control codes 1..26
  - Special keys: `BDOS_KEY_*` constants (base `0x100`) for arrows,
    Insert/Delete/Home/End/PageUp/PageDown, F1–F12.
- Real-time held-key bitmap (`bdos_key_state_bitmap`) is rebuilt from
  the raw HID report each poll; user programs read it via syscall
  `GET_KEY_STATE` (25). Bits: WASD, arrows, Space, Shift, Ctrl,
  Escape, E, Q.

## Filesystem

BRFS v2; flash target = `SPI_FLASH_1`. Cache is a contiguous buffer at
`MEM_BRFS_START`. Sync is explicit. See
[Docs/docs/Software/BRFS.md](../docs/Software/BRFS.md) for the full
description; the in-kernel side just opens the SPI-flash backend in
`fs.c` and exposes the result through the VFS.

## VFS

`Software/C/bdos/vfs.c` is the per-process file-descriptor layer. Four
device kinds:

- **file** — BRFS entry. Byte-addressable view; honours
  `O_CREAT`, `O_TRUNC`, `O_APPEND`.
- **tty** — `/dev/tty`. Cooked by default (line-buffered, ANSI emit
  on writes). With `O_RAW`, `read` returns 4-byte LE event packets;
  combine with `O_NONBLOCK` for polling games.
- **pipe** — temp file under `/tmp/`; the shell rewrites
  `a | b` into `a >/tmp/p.N ; b </tmp/p.N`.
- **null** — `/dev/null`.

Every spawned program inherits `fd 0/1/2 = /dev/tty`.

## Networking (FNP)

Custom L2 protocol over ENC28J60 (EtherType `0xB4B4`).

- MAC derived from SPI flash 0 unique ID (`02:B4:B4:00:00:XX`).
- Message types: `FILE_START`/`FILE_DATA`/`FILE_END`/`FILE_ABORT`
  (file transfer with checksum + ACK/NACK), `KEYCODE` (remote
  keyboard input), `MESSAGE`.
- `bdos_fnp_poll()` runs each main-loop iteration; user programs that
  call `NET_SEND`/`NET_RECV` take ownership and pause kernel polling
  until they exit.

## Heap

Bump allocator over `0x400000`–`0x1FFFFFF` (28 MiB). All allocations
are freed together when the owning program exits — no individual
`free()`. Programs that need to "reallocate" simply allocate a new
larger block and abandon the old one.

API (still **word-counted**, despite BRFS being byte-counted now):

```c
unsigned int *bdos_heap_alloc(unsigned int size_words);
void          bdos_heap_free_all(void);
```

User-facing `HEAP_ALLOC` (syscall 20) takes the same word count.

## Syscalls (Phase E set)

ABI: `r4` = number, `r5`/`r6`/`r7` = up to 3 args, `r1` = return
value. Dispatched in `bdos_syscall_dispatch()` (`syscall.c`). Numbers
that were retired in shell-terminal-v2 Phase E (0–3, 16–19, 22, 26,
31, 32) stay reserved and the dispatcher returns `-1` for them.

| #  | Name              | Args                                                  | Returns          |
|----|-------------------|-------------------------------------------------------|------------------|
| 4  | `FS_OPEN`         | `path`                                                | brfs fd          |
| 5  | `FS_CLOSE`        | `fd`                                                  | 0 ok             |
| 6  | `FS_READ`         | `fd, buf, words`                                      | words read       |
| 7  | `FS_WRITE`        | `fd, buf, words`                                      | words written    |
| 8  | `FS_SEEK`         | `fd, word_off`                                        | 0 ok             |
| 9  | `FS_STAT`         | `path, brfs_dir_entry*`                               | 0 ok             |
| 10 | `FS_DELETE`       | `path`                                                | 0 ok             |
| 11 | `FS_CREATE`       | `path`                                                | 0 ok             |
| 12 | `FS_FILESIZE`     | `fd`                                                  | size in words    |
| 13 | `SHELL_ARGC`      | —                                                     | argc             |
| 14 | `SHELL_ARGV`      | —                                                     | `char **argv`    |
| 15 | `SHELL_GETCWD`    | —                                                     | `char *cwd`      |
| 20 | `HEAP_ALLOC`      | `size_words`                                          | pointer / 0      |
| 21 | `DELAY`           | `ms`                                                  | 0                |
| 23 | `EXIT`            | `exit_code`                                           | *(no return)*    |
| 24 | `FS_READDIR`      | `path, brfs_dir_entry*, max`                          | entries          |
| 25 | `GET_KEY_STATE`   | —                                                     | bitmap           |
| 27 | `NET_SEND`        | `buf, len`                                            | 1 ok / 0 err     |
| 28 | `NET_RECV`        | `buf, max_len`                                        | bytes received   |
| 29 | `NET_PACKET_COUNT`| —                                                     | count            |
| 30 | `NET_GET_MAC`     | `6-int buf`                                           | 0                |
| 33 | `FS_MKDIR`        | `path`                                                | 0 ok             |
| 34 | `OPEN`            | `path, flags`                                         | fd               |
| 35 | `READ`            | `fd, buf, bytes`                                      | bytes read       |
| 36 | `WRITE`           | `fd, buf, bytes`                                      | bytes written    |
| 37 | `CLOSE`           | `fd`                                                  | 0 ok             |
| 38 | `LSEEK`           | `fd, off, whence`                                     | new offset       |
| 39 | `DUP2`            | `oldfd, newfd`                                        | newfd / -1       |
| 40 | `FS_FORMAT`       | `blocks, words/blk, label`                            | 0 ok             |

`OPEN`/`READ`/`WRITE`/`CLOSE`/`LSEEK`/`DUP2` (34–39) are the **byte-
oriented VFS** API and the path everything new should use. The older
word-oriented `FS_*` syscalls (4–12) talk directly to BRFS and are
kept for the in-kernel shell built-ins that haven't been migrated.

`EXIT` is special: it never returns to the caller. It resets the HW
stack to the trampoline depth and jumps to the BDOS return path,
unwinding the entire user program state.

`NET_SEND`/`NET_RECV` implicitly take ownership of the Ethernet
controller; the kernel FNP poll is paused until the owning program
exits.

## Program execution

Programs are launched by typing their name or path at the shell. The
shell tries built-ins first, then resolves the program (see
[Shell.md](../docs/Software/Shell.md) for the rules). Loading:

1. `bdos_slot_alloc()` finds a free slot.
2. The binary is read from BRFS in 256-word chunks into the slot's
   memory region.
3. If the file is larger than the header's `program_size`, a
   relocation table is appended; the loader walks it and patches
   data pointers, `load`/`loadhi` pairs, and header `jump`s by
   adding the slot base. See
   [Assembler — Relocatable Code](../docs/Software/Assembler.md#relocatable-code).
4. `ccache` flushes L1 I/D caches.
5. Register setup: `r13` = top of slot, `r15` = trampoline.
6. Jump to slot offset 0 (the relocated header `jump Main`).
7. On return, BDOS frees the slot (heap cleanup included) and prints
   the exit code.

argv lives in a per-process arena allocated on the kernel heap and
freed on program exit, so the child cannot corrupt the shell buffers.

## Job control

Up to 8 user programs in 2 MiB slots. At most one is RUNNING at a
time; others may be SUSPENDED. PIDs are user-visible (monotonically
increasing); `jobs`, `fg <pid>`, and `kill <pid>` use them.

Hotkeys during program execution:

- `F1`–`F8` — suspend the running program and switch to that slot.
- `F12` — suspend and return to BDOS.
- `Alt+F4` — kill and return to BDOS.

Suspend / resume work by saving/restoring the slot's hardware-stack
window plus its register file, switching to the BDOS stack between
runs.

## Shell

Bourne-style v2 shell — pipes (over temp files), redirection
(`<`, `>`, `>>`), boolean chains (`&&`, `||`, `;`), variable
expansion (`$VAR`, `${VAR}`), `#!/bin/sh` scripts. Built-in registry
in `shell_exec.c`; implementations in `shell_cmds.c`.

Built-ins: `help`, `clear`, `echo`, `uptime`, `pwd`, `cd`, `ls`,
`mkdir`, `mkfile`, `rm`, `cat`, `write`, `cp`, `mv`, `df`, `sync`,
`jobs`, `fg`, `kill`, `export`, `set`, `unset`, `env`, `exit`,
`true`, `false`. The previous `format` built-in moved to
`/bin/format` in Phase E (the boot-time mount-failure wizard still
lives in `shell_format.c`).

For the syntax reference and full built-in list see
[Shell.md](../docs/Software/Shell.md). For libterm v2 + supported
ANSI escapes (including DECAWM `?7h/l` for full-screen apps) see
[Terminal.md](../docs/Software/Terminal.md).

## Coding guidelines

- C11 with cproc/QBE limits: no inline asm, no `volatile`. Use
  `__builtin_load*` / `__builtin_store*` for MMIO.
- Each `.c` is compiled independently. Standard `#include <header.h>`.
- Assembly goes in dedicated `.asm` files
  (e.g. `slot_asm.asm`, `sys_asm.asm`).
- `TIMER_2` is taken by HID polling, `TIMER_3` by `delay()`. Use
  `TIMER_1` if you need a third (it doubles as the deferred
  ENC28J60 retry timer — coordinate).
- New shell built-ins: add the `bi_*` function in `shell_cmds.c`,
  register it in the `shell_exec.c` table, and (if user-facing) add
  the help line in `bi_help`.
- New syscalls: add the number in `bdos_syscall.h`, the case in
  `bdos_syscall_dispatch()`, the wrapper in `userlib/src/syscall.c`,
  and the prototype + (if needed) constants in
  `userlib/include/syscall.h`. Keep both headers in sync.
- Removed syscalls stay reserved (commented in the header, returning
  `-1` from the dispatcher) so old binaries fail loudly rather than
  silently misbehaving.

## User programs

`Software/C/userBDOS/`. Compiled with
`make compile-userbdos file=<name>` using the same modern toolchain
plus `crt0_ubdos.asm` and the `userlib` library. Output binary
lands in `Files/BRFS-init/bin/<name>`.

Reference ports for the shell-terminal-v2 API:
- [`snake.c`](../../Software/C/userBDOS/snake.c) — non-blocking raw
  TTY + ANSI rendering.
- [`edit.c`](../../Software/C/userBDOS/edit.c) — blocking raw TTY,
  alt-screen, palette-cached SGR, DECAWM-off rendering.

Programs that still use retired syscalls have a `FIXME:` header
comment with a per-API migration checklist; see them for examples
of what needs replacing.
