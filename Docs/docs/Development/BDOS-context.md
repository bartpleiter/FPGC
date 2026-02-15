# BDOS Context (for AI coding tools)

This document gives **only the BDOS-relevant context** needed to work effectively in this repo.

## What BDOS is (current state)

- BDOS is currently a **single binary kernel-style C program** built from `Software/C/BDOS/main.c`.
- It is in an early stage: boot/init + input/HID plumbing are implemented; broader OS services are still TODO.
- Build target uses B32CC with the `-bdos` mode.

## Build and run loop

- Compile: `make compile-bdos` (invokes `Scripts/BCC/compile_bdos.sh`).
- Run over UART: `make run-bdos`.
- Flash: `make flash-bdos`.

`compile_bdos.sh` pipeline:
1. Compile `Software/C/BDOS/main.c` -> `Software/ASM/Output/bdos.asm` using B32CC.
2. Assemble with ASMPY -> `Software/ASM/Output/code.list`.
3. Convert list bits -> `Software/ASM/Output/code.bin`.

## Important architectural constraints

- **No linker**: BDOS follows the project’s orchestrator pattern.
- `main.c` directly includes module `.c` files (`init.c`, `hid.c`, `fs.c`, `shell_cmds.c`, `shell.c`) for composition.
- `bdos.h` enables required libraries using `#define` flags before including `libs/kernel/kernel.h`.
- Keep code compatible with B32CC limits (simple C patterns, avoid advanced/complex constructs).

## BDOS runtime structure

### Entry and control flow

- `main()` calls `bdos_init()`, then performs BRFS boot init (`bdos_fs_boot_init()`), then enters `bdos_loop()` forever.
- `bdos_loop()` currently:
  - polls USB keyboard device state (`bdos_usb_keyboard_main_loop()`),
  - runs shell tick (`bdos_shell_tick()`) which consumes key events and drives command processing.

### BRFS boot policy

- BDOS uses `SPI_FLASH_1` for BRFS persistence.
- On boot, BDOS attempts `brfs_mount()`.
- If mount fails, shell startup asks user whether to format.
- Declining format triggers `bdos_panic()` because filesystem availability is required.
- Flash sync is explicit only: after successful format and when user runs `sync`.

### Interrupt model

- Global `interrupt()` dispatches by `get_int_id()`.
- `INTID_TIMER1` is used for USB keyboard polling via timer callbacks.
- Timer callbacks are registered in init (`timer_set_callback(TIMER_1, bdos_poll_usb_keyboard)`).

### Panic behavior

- `bdos_panic()` prints to terminal + UART, changes terminal palette, then halts.

## Input/HID subsystem (current implementation)

### Device and polling model

- USB keyboard host is CH376 (`bdos_usb_keyboard_spi_id`, currently default bottom CH376).
- `bdos_init_usb_keyboard()` initializes host and starts periodic polling every 10 ms.
- `bdos_usb_keyboard_main_loop()` handles connect/disconnect + enumeration lifecycle.

### Event abstraction for BDOS consumers

- HID code translates keyboard reports into **int key events** in a FIFO.
- Public API:
  - `int bdos_keyboard_event_available()`
  - `int bdos_keyboard_event_read()` (`-1` when empty)
- This means higher BDOS code does not parse raw HID scancodes directly.

### Event encoding

- Printable keys: ASCII value in low range (`int`).
- Control combos (e.g. Ctrl+A..Ctrl+Z): control codes (`1..26`).
- Non-printables: `BDOS_KEY_*` constants in `bdos.h` (arrows, Insert/Delete/Home/End/PageUp/PageDown, F1..F12), based at `BDOS_KEY_SPECIAL_BASE`.

### Repeat behavior

- Key repeat is implemented in HID polling with:
  - initial delay (`BDOS_KEY_REPEAT_DELAY_US`),
  - repeat interval (`BDOS_KEY_REPEAT_INTERVAL_US`).
- Repeat evaluation runs on polling ticks even when CH376 does not produce a new report.

### FIFO behavior

- Fixed-size ring buffer in `hid.c` (`BDOS_KEY_EVENT_FIFO_SIZE`, currently 64).
- On overflow, HID prints a UART warning and drops the new event.

## Memory map reference

- BDOS memory map constants are in `Software/C/BDOS/mem_map.h`.
- It defines kernel region, kernel heap, user program region (slot model), and BRFS cache region.
- Keep these in sync with compiler/backend assumptions (`cgb32p3.inc` note in file).

## Code locations to edit first

- API/defines/shared globals: `Software/C/BDOS/bdos.h`
- Boot/init and timer registration: `Software/C/BDOS/init.c`
- HID/input event pipeline: `Software/C/BDOS/hid.c`
- Main loop + interrupt dispatcher: `Software/C/BDOS/main.c`
- Filesystem integration: `Software/C/BDOS/fs.c`
- Shell command handlers + format wizard: `Software/C/BDOS/shell_cmds.c`
- Memory layout constants: `Software/C/BDOS/mem_map.h`

## Practical guidance for AI edits

- Prefer small, explicit C changes; avoid clever macros or complex abstractions.
- Preserve the single-compilation-unit pattern (`main.c` including BDOS modules).
- If adding periodic work, respect current timer ownership (`TIMER_1` already used for HID polling).
- For new keyboard features, keep FIFO API stable so higher layers remain decoupled from HID details.
