---
name: add-builtin
description: 'Add a new shell built-in command to BDOS. Use when asked to add a shell command, built-in, or CLI command.'
---
# Add a shell built-in command

Follow these steps exactly. Missing any step will cause a broken build
or a command that silently doesn't work.

## Step 1: Implement the command

Edit `Software/C/bdos/shell_cmds.c`.

Add a static implementation function following this pattern:

```c
static int bdos_shell_cmd_YOURCOMMAND(int argc, char **argv)
{
    // argc includes the command name itself
    // argv[0] is the command name
    if (argc < 2) {
        bdos_shell_print("Usage: YOURCOMMAND <arg>\n");
        return 1;
    }
    // Implementation here
    return 0; // 0 = success
}
```

Then add a public wrapper function:

```c
int bi_YOURCOMMAND(int argc, char **argv)
{
    return bdos_shell_cmd_YOURCOMMAND(argc, argv);
}
```

**Reference:** Study the `cd` builtin for a complete example.

## Step 2: Register in the builtin dispatch table

Edit `Software/C/bdos/shell_exec.c`.

Find the builtin dispatch section and add your command. The exact
registration mechanism is a string comparison chain — add yours
following the existing pattern.

## Step 3: Add help text

Edit `Software/C/bdos/shell_cmds.c`.

Find the `help` command implementation and add a line describing your
new command.

## Step 4: Declare the wrapper

If `bi_YOURCOMMAND` is called from `shell_exec.c`, ensure the
function is declared in the appropriate header or at the top of
`shell_exec.c` with an `extern` declaration.

## Step 5: Build and test

```
make compile-bdos
make test-shell-host    # If applicable
```

## Checklist
- [ ] Implementation function in `shell_cmds.c`
- [ ] Wrapper function `bi_YOURCOMMAND` in `shell_cmds.c`
- [ ] Registration in `shell_exec.c` dispatch table
- [ ] Help text added
- [ ] Declaration visible to `shell_exec.c`
- [ ] `make compile-bdos` passes
