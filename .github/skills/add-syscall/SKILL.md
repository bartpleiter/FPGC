---
name: add-syscall
description: 'Add a new syscall to BDOS and expose it to userland programs. Use when asked to add a system call, kernel API, or new OS functionality accessible from user programs.'
---
# Add a new syscall

Syscalls are the only way user programs communicate with the kernel.
Adding a syscall requires changes in 4–5 files across kernel and
userlib.

## Step 1: Define the syscall number

Edit `Software/C/kernel/include/syscall_nums.h`.

Add a new `#define SYS_YOURNAME <next_number>` in the appropriate
group. Use the next sequential number within its group.

## Step 2: Implement the kernel handler

Choose where to implement the actual logic:
- If it's a VFS operation → implement in `Software/C/kernel/src/vfs.c`
- If it's a filesystem operation → implement in `Software/C/kernel/src/fs.c`
- If it's a process operation → implement in `Software/C/kernel/src/proc.c`
- If it's a hardware operation → implement in the relevant driver
- If it's standalone → implement directly in `syscall.c`

## Step 3: Add dispatch case

Edit `Software/C/kernel/src/syscall.c`.

Add a case to the `syscall_dispatch` switch:

```c
case SYS_YOURNAME:
    return your_handler_function(a1, a2, a3);
```

Arguments `a1`, `a2`, `a3` are the three syscall parameters.
Return value goes back to the user program.

## Step 4: Add userlib wrapper

Edit `Software/C/userlib/src/syscall.c`:

```c
int sys_yourname(int arg1, int arg2)
{
    return syscall(SYS_YOURNAME, arg1, arg2, 0);
}
```

## Step 5: Declare in userlib header

Edit `Software/C/userlib/include/syscall.h`:

```c
int sys_yourname(int arg1, int arg2);
```

Also add the `#define SYS_YOURNAME <number>` here (must match
the kernel header).

## Step 6: Build and test

```
make compile-kernel            # Kernel with new syscall
make compile-userbdos-all      # All user programs (ensure nothing broke)
```

## Checklist
- [ ] Syscall number defined in `kernel/include/syscall_nums.h`
- [ ] Handler implemented (vfs.c / fs.c / proc.c / driver / syscall.c)
- [ ] Dispatch case added in `syscall.c`
- [ ] Userlib wrapper in `userlib/src/syscall.c`
- [ ] Userlib declaration in `userlib/include/syscall.h`
- [ ] Syscall number duplicated in userlib header
- [ ] `make compile-kernel` passes
- [ ] `make compile-userbdos-all` passes
