---
name: 'BDOS Kernel'
description: 'Rules for editing the BDOS operating system kernel'
applyTo: 'Software/C/bdos/**'
---
# BDOS kernel guidelines

## Validation
After any change: `make compile-bdos`

## File map
| File | Purpose |
|------|---------|
| `main.c` | Entry point, interrupt dispatcher, main loop |
| `init.c` | Boot sequence (hardware init, BRFS mount, SD mount) |
| `syscall.c` | Syscall dispatch switch (all userland→kernel calls) |
| `vfs.c` | Virtual filesystem (fd table, device dispatch, open/read/write/close) |
| `fs.c` | BRFS integration (format, sync, mount helpers) |
| `proc.c` | Process loading and execution |
| `slot.c` | Job slot management (6 slots × 2 MiB each) |
| `hid.c` | USB keyboard HID polling (INT# pin + Timer 1 ISR) |
| `eth.c` | Ethernet/FNP networking |
| `heap.c` | Kernel heap allocator |
| `shell.c` | Shell core (prompt, line editing, history) |
| `shell_cmds.c` | Built-in command implementations |
| `shell_exec.c` | Command execution (builtin dispatch, program loading, pipelines) |
| `shell_lex.c` | Shell lexer (tokenization) |
| `shell_parse.c` | Shell parser (AST construction) |
| `shell_vars.c` | Shell variable management ($VAR, export) |
| `shell_path.c` | PATH resolution |
| `shell_script.c` | Shell script execution |
| `shell_format.c` | Format command for shell |
| `shell_util.c` | Shell utility functions |

## Interrupt assignments
| ID | Source | Handler |
|----|--------|---------|
| 1 | UART | No-op |
| 2 | Timer 0 | Deferred network ISR |
| 3 | Timer 1 | USB keyboard polling (10 ms) |
| 4 | Timer 2 | General-purpose delay |
| 5 | Frame drawn | GPU vsync (no-op) |
| 6 | Ethernet | ENC28J60 packet receive |
| 7 | DMA | Defined but no handler yet (polled via `dma_busy()`) |

## Syscall table
Syscalls are dispatched in `syscall.c`. Current IDs:
`FS_FORMAT`, `SD_FORMAT`, `SHELL_ARGC`, `SHELL_ARGV`,
`SHELL_GETCWD`, `HEAP_ALLOC`, `DELAY`, `EXIT`, `GET_KEY_STATE`,
`NET_SEND`, `NET_RECV`, `NET_PACKET_COUNT`, `NET_GET_MAC`,
`OPEN`, `READ`, `WRITE`, `CLOSE`, `LSEEK`, `DUP2`,
`UNLINK`, `MKDIR`, `READDIR`

To add a new syscall, see the `/add-syscall` skill.

## Memory layout (from fpgc.h)
- `0x000000–0x400000` — Kernel code + stacks (4 MiB)
- `0x400000–0x2000000` — Kernel heap (28 MiB)
- `0x2000000–0x2C00000` — User program slots (6 × 2 MiB)
- `0x2C00000–0x3000000` — BRFS SD cache (4 MiB)
- `0x3000000–0x4000000` — BRFS SPI flash cache (16 MiB)

## Ripple effects
- Adding a syscall → also update `bdos_syscall.h`, userlib `syscall.c`, userlib `syscall.h`
- Changing VFS behavior → may affect all userBDOS programs
- Changing slot count/size → update `fpgc.h` constants
- Changing boot sequence → test on real hardware (no simulation available)

## Reference implementations
- New shell builtin → study `cd` in `shell_cmds.c`
- New syscall → study `SYSCALL_UNLINK` in `syscall.c` (most recently added)
