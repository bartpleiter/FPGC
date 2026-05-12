---
name: add-syscall
description: 'Add a new syscall to BDOS and expose it to userland programs. Use when asked to add a system call, kernel API, or new OS functionality accessible from user programs.'
---
# Add a new syscall

Syscalls are the only way user programs communicate with the BDOS
kernel. Adding a syscall requires changes in 4–5 files across kernel
and userlib.

## Step 1: Define the syscall number

Edit `Software/C/bdos/include/bdos_syscall.h`.

Add a new `#define SYSCALL_YOURNAME <next_number>` after the last
existing syscall. Use the next sequential number.

## Step 2: Implement the kernel handler

Choose where to implement the actual logic:
- If it's a VFS operation → implement in `Software/C/bdos/vfs.c`
- If it's a filesystem operation → implement in `Software/C/bdos/fs.c`
- If it's a hardware operation → implement in the relevant driver
- If it's standalone → implement directly in `syscall.c`

## Step 3: Add dispatch case

Edit `Software/C/bdos/syscall.c`.

Add a case to the `bdos_syscall_dispatch` switch:

```c
case SYSCALL_YOURNAME:
    return your_handler_function(a1, a2, a3);
```

Arguments `a1`, `a2`, `a3` are the three syscall parameters.
Return value goes back to the user program.

## Step 4: Add userlib wrapper

Edit `Software/C/userBDOS/userlib/src/syscall.c`:

```c
int yourname(int arg1, int arg2)
{
    return syscall(SYSCALL_YOURNAME, arg1, arg2, 0);
}
```

## Step 5: Declare in userlib header

Edit `Software/C/userBDOS/userlib/include/syscall.h`:

```c
int yourname(int arg1, int arg2);
```

Also add the `#define SYSCALL_YOURNAME <number>` here (must match
the kernel header).

## Step 6: Build and test

```
make compile-bdos              # Kernel with new syscall
make compile-userbdos-all      # All user programs (ensure nothing broke)
```

## Checklist
- [ ] Syscall number defined in `bdos/include/bdos_syscall.h`
- [ ] Handler implemented (vfs.c / fs.c / driver / syscall.c)
- [ ] Dispatch case added in `syscall.c`
- [ ] Userlib wrapper in `userBDOS/userlib/src/syscall.c`
- [ ] Userlib declaration in `userBDOS/userlib/include/syscall.h`
- [ ] Syscall number duplicated in userlib header
- [ ] `make compile-bdos` passes
- [ ] `make compile-userbdos-all` passes

## Reference
Study `SYSCALL_UNLINK` — it was the most recently added syscall and
shows the complete pattern across all files.
