# BDOS (OS)

BDOS is the operating system for the FPGC. It provides a shell, filesystem access (BRFS), Ethernet networking (FNP protocol), USB keyboard input, and the ability to load and run user programs.

## Memory Layout

BDOS organizes the FPGC's 16 MiW (64 MiB) SDRAM into four regions:

| Address Range | Size | Description |
|---------------|------|-------------|
| `0x000000` – `0x0FFFFF` | 1 MiW (4 MiB) | Kernel code, data, and stacks |
| `0x100000` – `0x7FFFFF` | 7 MiW (28 MiB) | Kernel heap (dynamic allocation) |
| `0x800000` – `0xBFFFFF` | 4 MiW (16 MiB) | User program slots |
| `0xC00000` – `0xFFFFFF` | 4 MiW (16 MiB) | BRFS filesystem cache |

### Kernel Region (0x000000)

Contains the BDOS binary (code + data). Two stacks grow downward from the top:

- **Main stack**: top at `0x0FBFFF`
- **Interrupt stack**: top at `0x0FFFFF`

### User Program Region (0x800000)

Divided into 8 slots of 512 KiW (2 MiB) each. Programs are loaded into slots and execute with their stack at the top of their allocated slot.

| Slot | Address Range | Stack Top |
|------|---------------|-----------|
| 0 | `0x800000` – `0x87FFFF` | `0x87FFFF` |
| 1 | `0x880000` – `0x8FFFFF` | `0x8FFFFF` |
| 2 | `0x900000` – `0x97FFFF` | `0x97FFFF` |
| ... | ... | ... |
| 7 | `0xB80000` – `0xBFFFFF` | `0xBFFFFF` |

Programs compiled with the B32CC `-user-bdos` flag and assembled with ASMPY `-h -i` are position-independent and can run in any slot.

## Shell

BDOS provides an interactive shell with the following commands:

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
| `write <file> <text>` | Write text to a file |
| `mkdir <path>` | Create a directory |
| `mkfile <path>` | Create an empty file |
| `rm <path>` | Remove a file or directory |
| `df` | Show filesystem usage |
| `sync` | Flush filesystem to flash |
| `format` | Format the filesystem (interactive wizard) |

## Program Loading and Execution

The `run` command loads a binary from BRFS into a user program slot and transfers control to it.

### Loading Process

1. **Path resolution**: If the argument has no `/`, BDOS looks in `/bin/` automatically
2. **Read binary**: The file is read from BRFS in 256-word chunks into slot 0 (`0x800000`)
3. **Cache flush**: The `ccache` instruction clears L1I/L1D caches to ensure the CPU fetches fresh instructions
4. **Register setup**:
      - `r13` (stack pointer) = top of slot (`0x87FFFF`)
      - `r15` (return address) = BDOS return trampoline
5. **Jump**: Execution transfers to offset 0 of the slot, which contains the ASMPY header `jump Main`
6. **Return**: When the program's `main()` returns (via `r15`), BDOS restores its own registers and prints the exit code

### Register Convention

| Register | Set by | Purpose |
|----------|--------|---------|
| `r0` | Hardware | Always zero |
| `r13` | BDOS loader | Stack pointer (top of slot) |
| `r14` | Program prologue | Base pointer (set to 0) |
| `r15` | BDOS loader | Return address (back to BDOS) |
| `r1` | Program | Return value from `main()` |

### Binary Format

User programs assembled with ASMPY `-h` have a 3-word header:

| Offset | Content |
|--------|---------|
| 0 | `jump Main` (relative, PIC: `jumpo`) |
| 1 | `jump Int` (PIC: `nop`) |
| 2 | Program length in words |

### Compiling User Programs

```bash
# Using B32CC + ASMPY:
b32cc userBDOS/program.c output.asm -user-bdos
asmpy output.asm output.list -h -i

# Or via the Makefile:
make fnp-upload-userbdos file=program
```

The `-user-bdos` flag tells B32CC to generate a minimal prologue (no stack setup — BDOS handles it) and no interrupt handler (BDOS handles interrupts). The `-i` flag tells ASMPY to generate position-independent code.

## Interrupt Handling

BDOS owns all hardware interrupts. The interrupt vector at address 1 points to BDOS's interrupt handler, which:

- Saves all registers to a dedicated interrupt stack (`0x0FFFFF`)
- Reads the interrupt ID
- Dispatches to the appropriate handler (USB keyboard polling on Timer 1, delay on Timer 2)
- Restores registers and returns via `reti`

User programs do not receive interrupts directly.

## Networking

BDOS includes an FNP (FPGC Network Protocol) handler that processes incoming Ethernet frames in the main loop. See [FNP](FNP.md) for usage details and [FNP Protocol Specification](../Development/FNP_Protocol.md) for the protocol design.

## Initialization

On boot, BDOS:

1. Initializes the GPU (text terminal, pattern/palette tables)
2. Configures Timer 1 (USB keyboard polling at 10ms) and Timer 2 (delay function)
3. Initializes the ENC28J60 Ethernet controller and stores its MAC address
4. Initializes the FNP protocol handler
5. Initializes the USB keyboard driver
6. Mounts BRFS from SPI flash into RAM cache
7. Starts the interactive shell
