---
name: 'userBDOS Programs'
description: 'Rules for editing user-space programs that run on BDOS'
applyTo: 'Software/C/userBDOS/**'
---
# userBDOS program guidelines

## Build & run
```
make compile-userbdos file=<name>          # Compile one program
make compile-userbdos-all                  # Compile all programs
make run-userbdos file=<name> [dev=N]      # Compile, upload, run via FNP
make fnp-upload-userbdos file=<name>       # Just upload (no run)
make fnp-debug-userbdos file=<name>        # Upload, run, capture UART output
```

## Program structure
Every userBDOS program is a single `.c` file (or a directory for
larger programs like `doom/`). Programs are loaded into job slots
(6 slots × 2 MiB each, starting at `0x2000000`).

```c
// Minimal userBDOS program
#include <syscall.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    // argc/argv come from the shell via SYSCALL_SHELL_ARGC/ARGV
    // Use syscall wrappers for all OS interaction
    return 0;
}
```

## Available syscalls (from userlib)
| Syscall | Wrapper | Purpose |
|---------|---------|---------|
| `OPEN` | `open(path, flags)` | Open file, returns fd |
| `READ` | `read(fd, buf, len)` | Read from fd |
| `WRITE` | `write(fd, buf, len)` | Write to fd |
| `CLOSE` | `close(fd)` | Close fd |
| `LSEEK` | `lseek(fd, offset, whence)` | Seek in file |
| `DUP2` | `dup2(oldfd, newfd)` | Duplicate fd |
| `UNLINK` | `unlink(path)` | Delete file |
| `MKDIR` | `mkdir(path)` | Create directory |
| `READDIR` | `readdir(path, buf, max)` | List directory |
| `EXIT` | `exit(code)` | Exit program |
| `DELAY` | `delay(us)` | Delay in microseconds |
| `HEAP_ALLOC` | `malloc(size)` | Allocate heap memory |

File flags: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`
Seek whence: `SEEK_SET`, `SEEK_CUR`, `SEEK_END`

## Available programs
`asm-link`, `bench`, `cmatrix`, `cpp`, `doom/`, `edit`, `format`,
`mbrot`, `mbrotc`, `mbroth`, `sdformat`, `snake`, `tetrisc`,
`tetrish`, `tree`, `w3d`

## Reference implementation
To create a new program, study `Software/C/userBDOS/tree.c` — it
shows file I/O, argument parsing, and proper exit handling.

## Ripple effects
- If you need a new syscall that doesn't exist → that requires kernel changes
  (see the `/add-syscall` skill)
- Changing userlib API → affects ALL userBDOS programs
