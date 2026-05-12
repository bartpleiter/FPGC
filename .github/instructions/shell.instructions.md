---
name: 'Shell'
description: 'Rules for editing the BDOS Bourne-style shell'
applyTo: 'Software/C/bdos/shell*.c'
---
# BDOS shell guidelines

## Validation
After any change: `make compile-bdos`
Shell-specific tests: `make test-shell-host`

## Architecture
The shell is a Bourne-style shell with: lex → parse → exec pipeline.

| File | Purpose |
|------|---------|
| `shell.c` | Core: prompt, line editing, history, `shell_tick()` |
| `shell_lex.c` | Lexer: tokenizes input into TOKEN_WORD, TOKEN_PIPE, etc. |
| `shell_parse.c` | Parser: builds command AST from tokens |
| `shell_exec.c` | Executor: dispatches builtins, loads programs, handles pipes/redirects |
| `shell_cmds.c` | Built-in command implementations |
| `shell_vars.c` | Variable management ($VAR, export, local vars) |
| `shell_path.c` | PATH resolution for external commands |
| `shell_script.c` | Script execution (.sh files) |
| `shell_format.c` | Format command |
| `shell_util.c` | Utility functions |

## Built-in commands
`help`, `clear`, `echo`, `uptime`, `pwd`, `cd`, `ls`, `mkdir`,
`mkfile`, `rm`, `cat`, `write`, `cp`, `mv`, `sync`, `df`, `jobs`,
`kill`, `fg`, `export`, `set`, `unset`, `env`, `exit`, `true`, `false`

## How to add a new built-in command

1. **Implement** in `shell_cmds.c`:
   ```c
   static int bdos_shell_cmd_mycommand(int argc, char **argv)
   {
       // Implementation
       return 0; // 0 = success, non-zero = error
   }
   ```

2. **Create wrapper** in `shell_cmds.c`:
   ```c
   int bi_mycommand(int argc, char **argv)
   {
       return bdos_shell_cmd_mycommand(argc, argv);
   }
   ```

3. **Register** in `shell_exec.c`: add to the builtin dispatch table

4. **Add help text** in the `help` command implementation in `shell_cmds.c`

## Reference implementation
Study the `cd` builtin in `shell_cmds.c` — it shows argument
parsing, error handling, and the wrapper pattern.

## Ripple effects
- Adding a builtin → edit `shell_cmds.c` AND `shell_exec.c`
- Changing the lexer → may break all command parsing
- Changing the parser AST → must update executor too
