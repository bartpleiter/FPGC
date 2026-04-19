# FPGC Project Context

This document is a high-level orientation file for AI coding tools
(GitHub Copilot, automated agents) working on the FPGC repo. Keep
edits short, factual, and link out to the canonical docs in
`Docs/docs/` rather than duplicating them here.

## What FPGC is

FPGC ("FPGA Computer") is a from-scratch computer system built around
a custom 32-bit RISC CPU implemented on FPGA. The repo contains
hardware, toolchain, and software:

- **CPU**: B32P3 — 5-stage pipelined, byte-addressable, 32-bit
  general-purpose registers, hardware fixed-point multiply/divide,
  optional FP64 coprocessor.
- **GPU**: tile + pixel framebuffer (320×240), VRAM-mapped.
- **Storage**: SDRAM with L1 I/D caches, SPI flash, BRFS filesystem
  (v2 — byte-native, RAM-cached, pluggable storage backend).
- **OS**: BDOS — a single-user, single-foreground OS with shell,
  filesystem, job control, USB-keyboard input, and ENC28J60 Ethernet.
- **Toolchain**: cproc (C11 frontend) → QBE (optimising backend) →
  ASMPY (Python assembler with linker). Self-hosting on BDOS via
  `cc` / `cc-min` shell scripts is operational.
- **Goal**: run Doom on real hardware — done. Future work: SD-card
  storage, more userBDOS programs, robust OS features, general extensions and improvements to the project -> going from custom POC to more established implementations like a proper terminal, C toolchain, FS layers, etc. to make porting existing code easier.

## Repo layout

```
Hardware/          FPGA Verilog + PCB
BuildTools/        cproc/, QBE/, ASMPY/
Software/
  ASM/             crt0 startup files + raw assembly programs
  C/
    libc/          picolibc-derived freestanding libc
    libfpgc/       hardware abstraction (drivers + libterm v2 + BRFS)
    bdos/          BDOS kernel sources
    userlib/       userland syscall wrappers + helpers
    userBDOS/      user programs (modern toolchain)
    bareMetal/     bare-metal test programs
Tests/             CPU sims, C tests, host tests
Scripts/           Build / programming / asset helpers
Docs/              MkDocs site (docs/) + plans (plans/) + this file
Files/             Default BRFS image staged into SPI flash
```

## CPU (B32P3)

5-stage pipeline (IF/ID/EX/MEM/WB). Byte-addressable
(`char`=1, `short`=2, `int`=4, pointer=4). 32-bit address space; 27-bit
jump constant gives 512 MiB of easily-jumpable instruction space.

| Region | Address range | Notes |
|--------|---------------|-------|
| SDRAM   | `0x00000000`–`0x03FFFFFF` | 64 MiB, cached |
| I/O     | `0x1C000000`–`0x1C0000FF` | MMIO peripherals |
| ROM     | `0x1E000000`–`0x1E000FFF` | 4 KiB; CPU resets here |
| VRAM32  | `0x1E400000`–`0x1E40107F` | pattern + palette tables |
| VRAM8   | `0x1E800000`–`0x1E808007` | tile + colour tables |
| VRAMPX  | `0x1EC00000`–`0x1EC4AFFF` | 320×240 pixel framebuffer |
| PC/HW   | `0x1F000000`–`0x1F000007` | PC backup + HW stack ptr |

16 GP registers (`r0`–`r15`); `r0` is hard-wired zero, `r13` = SP,
`r14` = FP, `r15` = RA. ABI: `r1` = return value, `r4`–`r7` = first
four args.

ISA reference: [Docs/docs/Hardware/ISA.md](../docs/Hardware/ISA.md).

## C toolchain

The toolchain pipeline:

```
src.c → cpp → cproc → QBE → b32p3 .asm → ASMPY linker → .list/.bin
```

- **cproc** ([BuildTools/cproc/](../../BuildTools/cproc/)) — C11
  frontend, emits QBE IR. Limitations: no inline asm, no `volatile`
  (use `__builtin_store{,b,h}` / `__builtin_load{,b,h}` for MMIO).
- **QBE** ([BuildTools/QBE/](../../BuildTools/QBE/)) — SSA optimiser
  + b32p3 backend.
- **ASMPY** ([BuildTools/ASMPY/](../../BuildTools/ASMPY/)) — Python
  assembler with an assembly-level linker, ELF-style data directives,
  pseudo-instructions (`load32`, `addr2reg`), and reloc-table support
  for PIC user programs.

Build orchestration is `Scripts/BCC/compile_modern_c.sh`, invoked by
the top-level `Makefile` (`make compile-bdos`,
`make compile-userbdos file=<n>`, `make compile-userbdos-all`,
`make compile-bootloader`, etc.).

Self-hosting on BDOS is operational: `cc` and `cc-min` shell scripts
drive cproc + QBE + ASMPY directly on the FPGC, with assembly outputs
cached under `/lib/asm-cache/`. See
[Docs/plans/asm-selfhost.md](../plans/asm-selfhost.md) and
[Docs/plans/selfhost-modern-c.md](../plans/selfhost-modern-c.md).

## BRFS v2 (filesystem)

- Byte-native API (`brfs_read`/`brfs_write` take byte counts;
  on-disk `filesize` is bytes).
- FAT-based with per-file linked chains.
- RAM cache over a `brfs_storage_t` vtable; today only the SPI-flash
  backend ships. SD-card backend is a v2.1 follow-up.
- LE on disk; magic `BRF2`; superblock version `2`.
- Persistence is explicit (`brfs_sync()`); a power loss between syncs
  rolls back to the last synced state.
- See [BRFS docs](../docs/Software/BRFS.md) and
  [BRFS-v2 plan](../plans/BRFS-v2.md).

## BDOS

Single-foreground OS with the v2 shell + libterm v2 terminal that
landed in 2025 ([shell-terminal-v2 plan](../plans/shell-terminal-v2.md)).
See [BDOS-context.md](BDOS-context.md) for the full source-file map,
syscall table, and subsystem detail.

Key facts useful for any change:

- All terminal I/O is ANSI-escape-driven on `fd 1`. There is no
  per-cell tile syscall any more — programs `sys_write(1, "\x1b[..."`.
- Raw keyboard events come from `/dev/tty` opened with
  `O_RDONLY|O_RAW[|O_NONBLOCK]`; each `read` returns 4 bytes
  (little-endian event code: ASCII or `KEY_*`). See `snake.c` and
  `edit.c` for reference ports.
- `format` is a userland program (`/bin/format`) — not a built-in.
- VFS exposes file / `/dev/tty` / `/dev/null` / pipe under one
  `open`/`read`/`write`/`close`/`lseek`/`dup2` API.

## User programs

Located in [Software/C/userBDOS/](../../Software/C/userBDOS/). All
new programs use:

```c
#include <syscall.h>

int main(void)
{
    sys_putstr("Hello from userBDOS!\n");   /* fd 1 */
    return 0;
}
```

`sys_putstr` / `sys_putc` are convenience wrappers over
`sys_write(1, ...)`; redirection (`>`, `>>`, `|`) just works because
the shell rewrites `fd 1`/`fd 0` before calling the program.

Build: `make compile-userbdos file=<name>`. Output binary lands in
`Files/BRFS-init/bin/<name>` and can be staged into the BRFS image.

Some shell-terminal-v2 ports may still be incomplete; see the FIXME
header comment at the top of each broken `userBDOS/*.c` for the
migration checklist.

## Testing

| Make target | What it runs |
|-------------|--------------|
| `make test-cpu`        | All CPU Verilog testbenches |
| `make test-c`          | All cproc+QBE+ASMPY end-to-end tests |
| `make test-asmpy`      | Python unit tests for ASMPY |
| `make test-host`       | Host-side BDOS / shell unit tests under `Tests/host/` |

Single-test variants exist (`make test-cpu-single file=…`,
`make test-c-single file=…`)
and there are debug variants that pipe Verilog `$display` output
(`make debug-cpu file=…`).

When working on:

- Verilog → `make test-cpu` then `make test-c`.
- Modern toolchain (cproc/QBE/ASMPY) → `make test-c`.
- BDOS or a userBDOS program → just compile (`make compile-bdos` or
  `make compile-userbdos file=<n>`); there is no per-feature test
  harness yet beyond the host tests.

## House style

- C11 with cproc/QBE limits (no inline asm, no `volatile`). Use
  `__builtin_load*` / `__builtin_store*` for MMIO.
- Each `.c` file is compiled independently; standard
  `#include <header.h>` with `-I` paths set by the build script.
- New assembly goes in dedicated `.asm` files alongside the C — never
  inline.
- Keep header changes in lock-step: the BDOS `bdos_syscall.h` and the
  userland `syscall.h` must match. Removed syscalls stay reserved
  (commented in the header, returning `-1` from the dispatcher).

## Where to look next

- [Docs/docs/](../docs/) — mkdocs site, the canonical user-facing docs.
- [Docs/plans/](../plans/) — design plans for in-flight work
  (`shell-terminal-v2.md`, `BRFS-v2.md`, `asm-selfhost.md`,
  `selfhost-modern-c.md`, …).
- [Docs/context/BDOS-context.md](BDOS-context.md) — deep dive into
  BDOS sources, syscalls, slots, hotkeys.
