---
name: 'Kernel'
description: 'Rules for editing the BDOS kernel'
applyTo: 'Software/C/kernel/**'
---
# Kernel guidelines

## Validation
After any change: `make compile-kernel`

## File map
| File | Purpose |
|------|---------|
| `main.c` | Entry point, interrupt dispatcher, kernel loop |
| `init.c` | Boot sequence (hardware init, BRFS mount, SD mount) |
| `syscall.c` | Syscall dispatch (all userland→kernel calls) |
| `vfs.c` | Virtual filesystem (fd table, device dispatch, open/read/write/close) |
| `fs.c` | BRFS integration (format, sync, mount helpers) |
| `proc.c` | Process table, spawn, exit, waitpid |
| `sched.c` | Round-robin scheduler, sleep/wake, context dispatch |
| `mem.c` | Process memory allocator (variable-size regions) |
| `hid.c` | USB keyboard HID polling (INT# pin + Timer 1 ISR) |
| `net.c` | Network subsystem (ENC28J60 packet ring) |
| `fnp.c` | FNP file-transfer protocol handler |
| `dev.c` | Device registration table |
| `dev_tty.c` | /dev/tty device (cooked/raw modes, UART mirror) |
| `dev_null.c` | /dev/null device |
| `dev_pixpal.c` | /dev/pixpal GPU palette device |
| `dev_uart.c` | /dev/uart raw serial device |
| `dev_random.c` | /dev/random LFSR pseudo-random device |
| `dev_proc.c` | /proc virtual filesystem (uptime, meminfo, ps, df) |

## Interrupt assignments
| ID | Source | Handler |
|----|--------|---------|
| 1 | UART | Ring buffer fill |
| 2 | Timer 0 | Deferred network ISR |
| 3 | Timer 1 | USB keyboard polling (10 ms) |
| 4 | Timer 2 | Delay completion |
| 5 | Frame drawn | (unused) |
| 6 | Ethernet | ENC28J60 packet receive |
| 7 | DMA | Transfer done notification |

## Syscall table
Syscalls are dispatched in `syscall.c` with POSIX-aligned numbering.
Numbers defined in `include/syscall_nums.h`:
- Process control (1–6): EXIT, YIELD, SPAWN, WAITPID, GETPID, KILL
- File I/O (10–15): OPEN, CLOSE, READ, WRITE, LSEEK, DUP2
- Filesystem (20–24): UNLINK, MKDIR, READDIR, RENAME, STAT
- Info (30–31): ARGC, ARGV
- System (34–36): SBRK, GETCWD, CHDIR
- Format (38–39): FS_FORMAT, SD_FORMAT
- Hardware (40–43): GET_KEY_STATE, IOCTL, SLEEP, GET_MICROS
- Network (50–53): NET_SEND, NET_RECV, NET_PACKET_COUNT, NET_GET_MAC

## Memory layout (from fpgc.h)
- `0x000000–0x0FFFFF` — Kernel code (1 MiB)
- `0x100000–0x10FFFF` — Kernel stacks (64 KiB)
- `0x110000–0x1FFFFF` — Kernel heap (~960 KiB)
- `0x200000–0x1FFFFFF` — Process memory pool (30 MiB)
- `0x2000000–0x23FFFFF` — BRFS SD cache (4 MiB LRU)
- `0x2400000–0x3FFFFFF` — BRFS SPI flash cache (28 MiB)

## Ripple effects
- Adding a syscall → also update `syscall_nums.h`, userlib `syscall.c`, userlib `syscall.h`
- Changing VFS behavior → may affect all userBDOS programs
- Changing memory layout → update `fpgc.h` constants
- Changing boot sequence → test on real hardware

## Reference implementations
- New device driver → study `dev_null.c` (simplest) or `dev_uart.c`
- New syscall → study a recent syscall in `syscall.c`
