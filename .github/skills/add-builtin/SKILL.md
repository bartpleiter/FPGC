---
name: add-builtin
description: 'Add a new shell built-in command to BDOS. Use when asked to add a shell command, built-in, or CLI command.'
---
# Add a shell built-in command

The shell is a userBDOS program at `Software/C/userBDOS/sh.c`.
Built-in commands are implemented directly in `sh.c`.

## Step 1: Implement the command

Edit `Software/C/userBDOS/sh.c`.

Add an implementation function following the existing pattern:

```c
static int bi_YOURCOMMAND(int argc, char **argv)
{
    // argc includes the command name itself
    // argv[0] is the command name
    if (argc < 2) {
        print_str("Usage: YOURCOMMAND <arg>\n");
        return 1;
    }
    // Implementation here
    return 0; // 0 = success
}
```

## Step 2: Register in the builtin dispatch table

Find the builtin dispatch section in `sh.c` and add your command.
The registration mechanism is a string comparison chain — add yours
following the existing pattern.

## Step 3: Add help text

Find the `help` command implementation in `sh.c` and add a line
describing your new command.

## Step 4: Build and test

```
make compile-userbdos file=sh
```

## Checklist
- [ ] Implementation function in `sh.c`
- [ ] Registration in builtin dispatch table
- [ ] Help text added
- [ ] `make compile-userbdos file=sh` passes

## Note: External commands

For commands that should be standalone programs (pipe-compatible,
usable outside the shell), create a new userBDOS program in
`Software/C/userBDOS/` instead. See existing programs like `ls.c`,
`cat.c`, `grep.c` for examples. External programs are preferred
over builtins for most new commands.
