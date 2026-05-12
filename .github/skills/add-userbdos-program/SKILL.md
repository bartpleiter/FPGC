---
name: add-userbdos-program
description: 'Create a new userBDOS program (user-space application for BDOS). Use when asked to write a new program, app, tool, or utility for the FPGC.'
---
# Create a new userBDOS program

## Step 1: Create the source file

Create `Software/C/userBDOS/YOURPROGRAM.c`:

```c
/*
 * YOURPROGRAM — brief description
 *
 * Usage: YOURPROGRAM [args]
 */

// Standard includes for userBDOS programs
#include <syscall.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    // argc/argv are provided by the shell via syscalls
    // Use syscall wrappers for all OS interaction:
    //   open(), read(), write(), close(), lseek()
    //   mkdir(), unlink(), readdir()
    //   malloc(), delay(), exit()

    return 0;
}
```

## Step 2: Build

```
make compile-userbdos file=YOURPROGRAM
```

This produces a flat binary that can be uploaded to the FPGC.

## Step 3: Upload and run

```
make run-userbdos file=YOURPROGRAM [dev=N]      # Compile + upload + run
make fnp-upload-userbdos file=YOURPROGRAM        # Just upload
make fnp-debug-userbdos file=YOURPROGRAM         # Upload + run + UART capture
```

## Available APIs

### File I/O
```c
int fd = open("/path/to/file", O_RDONLY);
int n = read(fd, buf, sizeof(buf));
write(fd, buf, n);
lseek(fd, 0, SEEK_SET);
close(fd);
```

Flags: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`

### Directory operations
```c
mkdir("/path/to/dir");
unlink("/path/to/file");
readdir("/path", entries_buf, max_entries);
```

### Memory
```c
void *p = malloc(size);  // Kernel heap allocation (no free!)
```

### Output
```c
// Write to stdout (fd 1) — goes to the terminal
write(1, "Hello\n", 6);

// Or use stdlib helpers if available
```

### Keyboard input
```c
// Read from stdin (fd 0)
char c;
read(0, &c, 1);
```

## Notes
- Programs run in a 2 MiB slot (one of 6 job slots)
- No dynamic linking — everything is statically compiled
- No `free()` — allocated memory is released when the program exits
- No `printf` — use `write(1, ...)` or stdlib string helpers
- File paths: `/` for SPI flash root, `/sdcard/` for SD card

## Reference
Study `Software/C/userBDOS/tree.c` for a clean example with file I/O
and argument handling.
