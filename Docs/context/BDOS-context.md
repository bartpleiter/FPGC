# BDOS Context (for AI coding tools)

BDOS is the kernel/OS for FPGC. Built from separately-compiled C +
assembly sources using the modern toolchain (cproc → QBE → ASMPY →
flat binary). For the project-wide overview see
[Project-context.md](Project-context.md); for end-user docs see
[Docs/docs/Software/](../docs/Software/).

## Build & run

| Target | Purpose |
|--------|---------|
| `make compile-bdos` | Compile BDOS kernel (~302 KB binary) |
| `make run-bdos` | Compile + upload via UART |
| `make flash-bdos` | Flash to SPI (persistent) |
| `make compile-userbdos file=<name>` | Compile one userBDOS program |
| `make compile-userbdos-all` | Compile all ~18 userBDOS programs |

Pipeline: `crt0_bdos.asm + libc + libfpgc + bdos sources → cproc →
QBE → asm → ASMPY linker → .list → .bin`.

## Source layout

### libc (`Software/C/libc/`)

picolibc-derived freestanding C library: `string.h`, `stdlib.h`,
`stdio.h` (printf family), `ctype.h`, `stdint.h`, `errno.h`, ...
System hooks in `sys/`. printf goes through `_write` which the BDOS
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

### BDOS kernel (`Software/C/bdos/`)

| File | Role |
|------|------|
| `include/bdos.h` | Master include — pulls libc, libfpgc, all bdos_* headers |
| `include/bdos_syscall.h` | Syscall numbers (active + reserved) |
| `include/bdos_mem_map.h` | Memory layout constants (slots, caches, stacks) |
| `include/bdos_heap.h` | Heap allocator interface |
| `include/bdos_slot.h` | Slot management (loader / runner) |
| `include/bdos_proc.h` | PID table + per-process FD table |
| `include/bdos_vfs.h` | Byte-oriented VFS API |
| `include/bdos_hid.h` | USB keyboard subsystem + key-state bitmap |
| `include/bdos_fs.h` | BRFS mount/format/sync + SD card globals |
| `include/bdos_fnp.h` | FNP protocol definitions |
| `include/bdos_shell.h` | Shell entry points + argc/argv globals |
| `main.c` | Entry point, interrupt handler, main loop |
| `init.c` | Hardware init (GPU, UART, timers, SPI, USB, Ethernet) |
| `syscall.c` | Syscall C dispatcher (single switch) |
| `heap.c` | Bump allocator for kernel heap |
| `slot.c` + `slot_asm.asm` | Program-slot loader, context switch, exec/return |
| `proc.c` | PID table + per-process state |
| `vfs.c` | File/tty/null/pipe/pixpal device table; per-process fds |
| `hid.c` | USB keyboard: INT# polling, HID translation, FIFO, key state |
| `fs.c` | BRFS mount/format/sync for both SPI flash and SD card |
| `eth.c` | FNP file-transfer + remote-keycode injection |
| `shell.c` | Line editor, prompt, command dispatch |
| `shell_lex.c` | Tokenizer (quoting, operators, escapes) |
| `shell_parse.c` | AST builder (commands, pipelines, chains, redirs) |
| `shell_exec.c` | Built-in registry + program launcher; pipes via temp files |
| `shell_path.c` | `/bin/<name>` then cwd lookup |
| `shell_script.c` | `#!/bin/sh` interpreter (`$0`–`$9`, `$#`, `$?`, `set -e`) |
| `shell_vars.c` | Shell + environment variables |
| `shell_cmds.c` | Built-in implementations (`bi_*`) |
| `shell_format.c` | Boot-time mount-failure format wizard |
| `shell_util.c` | Misc shell helpers |

Each `.c` is compiled independently; standard
`#include <header.h>` with `-I` paths set by the build script.

## Memory map (byte addresses)

| Region | Range | Size |
|--------|-------|------|
| Kernel code + stacks | `0x000000`–`0x3FFFFF` | 4 MiB |
| Kernel heap | `0x400000`–`0x1FFFFFF` | 28 MiB |
| User program slots | `0x2000000`–`0x2BFFFFF` | 12 MiB (6 × 2 MiB) |
| SD card BRFS cache | `0x2C00000`–`0x2FFFFFF` | 4 MiB (LRU) |
| SPI flash BRFS cache | `0x3000000`–`0x3FFFFFF` | 16 MiB |

Kernel stacks: main `0x3DFFFC`, syscall `0x3EFFFC`, interrupt
`0x3FFFFC`.

## Boot flow

1. `main()` → `bdos_init()` — GPU + libterm, UART, timers, SPI,
   USB keyboard (CH376 with boot-time enumeration), Ethernet
   (ENC28J60).
2. `bdos_fs_boot_init()` — `brfs_init()` then `brfs_mount()` on
   SPI flash. On mount failure sets `bdos_fs_boot_needs_format`.
3. `bdos_fs_sd_init()` — `sd_init()` then `brfs_init()` +
   `brfs_mount()` on SD card. Mounts at `/sdcard` if card present;
   silently skipped if no card.
4. `bdos_shell_init()` — banner; if SPI flash mount failed, run
   the in-kernel format wizard from `shell_format.c`.
5. `bdos_loop()` — forever: poll keyboard (INT# pin check +
   main-loop connect/disconnect), poll FNP/Ethernet, run
   `bdos_shell_tick()`.

## Interrupts

`interrupt()` reads `INTID` and dispatches:

| INT ID | Source | Handler |
|--------|--------|---------|
| 1 | UART RX | Ring buffer fill |
| 2 | Timer 0 | Deferred ENC28J60 ISR retry (SPI was busy) |
| 3 | Timer 1 | USB keyboard HID report polling (10 ms periodic) |
| 4 | Timer 2 | `delay()` completion |
| 5 | Frame drawn | (unused) |
| 6 | ENC28J60 RX | Drain hardware RX into 64-slot kernel ring |
| 7 | DMA complete | Transfer done notification |

## Input / HID

USB keyboard via CH376 over SPI. Hybrid polling approach:

- **Connect/disconnect**: main-loop polling of the CH376 INT# pin
  via `ch376_read_int()` — reads MMIO directly, no SPI traffic when
  idle. Eliminates periodic activity LED blinks.
- **HID report reading**: Timer 1 ISR callback (10 ms). When a
  keyboard is connected, `ch376_read_keyboard()` reads the HID
  report and pushes events into the FIFO.
- **Boot-time enumeration**: `bdos_init_usb_keyboard()` attempts
  `ch376_test_connect()` + `ch376_enumerate_device()` during boot,
  so a keyboard plugged in at power-on is immediately available.

Event FIFO: 64-entry ring; overflow drops new events. Consumers:
`bdos_keyboard_event_available()` / `bdos_keyboard_event_read()`.

Key encoding (FIFO and 4-byte `/dev/tty` packets):
- Printable ASCII: raw value
- Ctrl+A..Z: control codes 1..26
- Special keys: `BDOS_KEY_*` / `KEY_*` constants (base `0x100`)

Real-time held-key bitmap (`bdos_key_state_bitmap`) rebuilt from raw
HID report each poll; user programs read via syscall `GET_KEY_STATE`
(25). Bits: WASD, arrows, Space, Shift, Ctrl, Escape, E, Q.

## Filesystem

Two BRFS v2 instances, each backed by a different storage vtable:

| Instance | Backend | Mount point | Cache |
|----------|---------|------------|-------|
| `brfs_spi` | SPI flash 0 | `/` (root) | 16 MiB direct-mapped |
| `brfs_sd` | SD card (SPI5) | `/sdcard` | 4 MiB LRU |

- `bdos_fs_for_path(path)` routes paths starting with `/sdcard/` to
  `brfs_sd`; everything else goes to `brfs_spi`.
- Shell `ls /` injects a synthetic `/sdcard` entry when
  `bdos_sd_initialized` is true.
- Sync is explicit (`brfs_sync()`).
- Format: `bdos_fs_sd_format_and_sync()` for SD card; syscall
  `SD_FORMAT` (41) exposes this to userland (`sdformat` program).
- See [BRFS docs](../docs/Software/BRFS.md).

## VFS

`Software/C/bdos/vfs.c` — per-process file-descriptor layer. Five
device kinds:

- **file** — BRFS entry (on SPI flash or SD card). Byte-addressable;
  honours `O_CREAT`, `O_TRUNC`, `O_APPEND`.
- **tty** — `/dev/tty`. Cooked by default (line-buffered, ANSI on
  writes). With `O_RAW`, `read` returns 4-byte LE event packets;
  combine with `O_NONBLOCK` for polling games.
- **pipe** — temp file under `/tmp/`; the shell rewrites `a | b`
  into `a >/tmp/p.N ; b </tmp/p.N`.
- **null** — `/dev/null`.
- **pixpal** — `/dev/pixpal`. 256-entry × 4-byte 8-bit pixel-palette
  DAC. `lseek` sets byte cursor; `write` autoincrements one 4-byte
  `0x00RRGGBB` entry. Both length and cursor must be 4-byte aligned.

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
freed together when the owning program exits — no individual `free()`.

API (word-counted):

```c
unsigned int *bdos_heap_alloc(unsigned int size_words);
void          bdos_heap_free_all(void);
```

User-facing `HEAP_ALLOC` (syscall 20) takes the same word count.

## Syscalls

ABI: `r4` = number, `r5`/`r6`/`r7` = up to 3 args, `r1` = return
value. Dispatched in `bdos_syscall_dispatch()` (`syscall.c`).
Reserved numbers return `-1`.

| # | Name | Args | Returns |
|---|------|------|---------|
| 0–12 | *(reserved — legacy raw I/O and raw BRFS)* | | `-1` |
| 13 | `SHELL_ARGC` | — | argc |
| 14 | `SHELL_ARGV` | — | `char **argv` |
| 15 | `SHELL_GETCWD` | — | `char *cwd` |
| 16–19 | *(reserved — legacy terminal)* | | `-1` |
| 20 | `HEAP_ALLOC` | `size_words` | pointer / 0 |
| 21 | `DELAY` | `ms` | 0 |
| 22 | *(reserved)* | | `-1` |
| 23 | `EXIT` | `exit_code` | *(no return)* |
| 24 | *(reserved)* | | `-1` |
| 25 | `GET_KEY_STATE` | — | bitmap |
| 26 | *(reserved)* | | `-1` |
| 27 | `NET_SEND` | `buf, len` | 1 ok / 0 err |
| 28 | `NET_RECV` | `buf, max_len` | bytes received |
| 29 | `NET_PACKET_COUNT` | — | count |
| 30 | `NET_GET_MAC` | `6-int buf` | 0 |
| 31–33 | *(reserved)* | | `-1` |
| 34 | `OPEN` | `path, flags` | fd |
| 35 | `READ` | `fd, buf, bytes` | bytes read |
| 36 | `WRITE` | `fd, buf, bytes` | bytes written |
| 37 | `CLOSE` | `fd` | 0 ok |
| 38 | `LSEEK` | `fd, off, whence` | new offset |
| 39 | `DUP2` | `oldfd, newfd` | newfd / -1 |
| 40 | `FS_FORMAT` | `blocks, words/blk, label` | 0 ok |
| 41 | `SD_FORMAT` | `blocks, words/blk, label` | 0 ok |
| 42 | `UNLINK` | `path` | 0 ok |
| 43 | `MKDIR` | `path` | 0 ok |
| 44 | `READDIR` | `path, entry_buf, max` | entries |

The **byte-oriented VFS** API (34–39, 42–44) is the path everything
new should use. `OPEN` flags: `O_RDONLY` (1), `O_WRONLY` (2),
`O_RDWR` (3), `O_APPEND` (4), `O_CREAT` (8), `O_TRUNC` (16),
`O_RAW` (32), `O_NONBLOCK` (64).

`EXIT` never returns — it resets the HW stack and jumps to the BDOS
return path. `NET_SEND`/`NET_RECV` take ownership of the Ethernet
controller while active.

## Program execution

Programs are launched by typing their name or path at the shell.
Loading:

1. `bdos_slot_alloc()` finds a free slot (6 slots, 2 MiB each).
2. Binary read from BRFS in 256-word chunks into slot memory.
3. If file has a relocation table, the loader patches data pointers,
   `load`/`loadhi` pairs, and header `jump`s by adding the slot base.
4. `ccache` flushes L1 I/D caches.
5. Register setup: `r13` = top of slot, `r15` = trampoline.
6. Jump to slot offset 0 (relocated header `jump Main`).
7. On return, BDOS frees the slot (heap cleanup included) and prints
   the exit code.

## Job control

Up to 6 user programs in 2 MiB slots. At most one RUNNING; others
may be SUSPENDED. PIDs are user-visible (monotonically increasing);
`jobs`, `fg <pid>`, and `kill <pid>` use them.

Hotkeys during program execution:

- `F1`–`F6` — suspend and switch to that slot.
- `F12` — suspend and return to BDOS.
- `Alt+F4` — kill and return to BDOS.

## Shell

Bourne-style v2 shell — pipes (over temp files), redirection
(`<`, `>`, `>>`), boolean chains (`&&`, `||`, `;`), variable
expansion (`$VAR`, `${VAR}`), `#!/bin/sh` scripts.

Built-ins: `help`, `clear`, `echo`, `uptime`, `pwd`, `cd`, `ls`,
`mkdir`, `mkfile`, `rm`, `cat`, `write`, `cp`, `mv`, `df`, `sync`,
`jobs`, `fg`, `kill`, `export`, `set`, `unset`, `env`, `exit`,
`true`, `false`.

`format` is an **external program** (`/bin/format`), not a built-in.
The boot-time mount-failure wizard still lives in `shell_format.c`.

## User programs

`Software/C/userBDOS/`. Compiled with
`make compile-userbdos file=<name>` (or `make compile-userbdos-all`).
Output: `Files/BRFS-init/bin/<name>`.

| Program | Description |
|---------|-------------|
| `doom/` | Full Doom port (DMA-accelerated) |
| `w3d.c` | Wolfenstein 3D raycaster |
| `edit.c` | Text editor (alt-screen, raw TTY) |
| `snake.c` | Snake game (non-blocking raw TTY) |
| `tetrisc.c` / `tetrish.c` | Tetris (client / host) |
| `mbrot.c` / `mbrotc.c` / `mbroth.c` | Mandelbrot (solo / cluster) |
| `cmatrix.c` | CMatrix display |
| `tree.c` | Recursive directory listing |
| `bench.c` | Benchmark suite |
| `format.c` | SPI flash BRFS format |
| `sdformat.c` | SD card BRFS format |
| `asm-link.c` | On-device assembler/linker |
| `cpp.c` | On-device C preprocessor |

Reference ports: `snake.c` (non-blocking raw TTY + ANSI),
`edit.c` (blocking raw TTY, alt-screen, DECAWM-off).

## Coding guidelines

- C11 with cproc/QBE limits: no inline asm, no `volatile`. Use
  `__builtin_load*` / `__builtin_store*` for MMIO.
- Each `.c` is compiled independently. Standard `#include <header.h>`.
- Assembly goes in dedicated `.asm` files.
- Timer 0 is free (doubles as deferred ENC28J60 retry). Timer 1 is
  taken by HID polling. Timer 2 is taken by `delay()`.
- DMA transfers must be 32-byte aligned. Call `cache_flush_data()`
  before MEM→device and after device→MEM.
- SD card is on SPI bus 5 (`FPGC_SPI_SD_CARD`). Use `sd.h` API.
- New shell built-ins: add `bi_*` in `shell_cmds.c`, register in
  `shell_exec.c` table, add help line in `bi_help`.
- New syscalls: add number in `bdos_syscall.h`, case in
  `bdos_syscall_dispatch()`, wrapper in `userlib/src/syscall.c`,
  prototype in `userlib/include/syscall.h`. Keep both headers in sync.
- Removed syscalls stay reserved (return `-1`).
