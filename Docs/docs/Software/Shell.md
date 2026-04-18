# Shell

The BDOS v2 shell is a Bourne-style command interpreter that drops in once
[BDOS](OS.md) finishes booting. It is the "shell" half of the
[shell-terminal-v2](../../plans/shell-terminal-v2.md) redesign.

It is split across several source files in `Software/C/bdos/`:

| File | Role |
|------|------|
| `shell.c` | Line editor, prompt rendering, command dispatch |
| `shell_lex.c` | Tokenizer (quoting, operators, escapes) |
| `shell_parse.c` | Builds a small AST (commands, pipelines, chains, redirs) |
| `shell_exec.c` | Built-in registry + program launcher; pipes via temp files |
| `shell_path.c` | `PATH` lookup (`/bin/<name>` then cwd) |
| `shell_script.c` | In-kernel `#!/bin/sh` interpreter (`$0`–`$9`, `$#`, `$?`, `set -e`) |
| `shell_vars.c` | Shell + environment variable storage |
| `shell_cmds.c` | Built-in implementations (the `bi_*` functions) |
| `shell_format.c` | Boot-time mount-failure format wizard (only) |

## Syntax

```
COMMAND       := WORD WORD* (REDIR)*
PIPELINE      := COMMAND ('|' COMMAND)*
CHAIN         := PIPELINE (('&&' | '||' | ';') PIPELINE)*
REDIR         := ('<' WORD) | ('>' WORD) | ('>>' WORD)
```

### Words and quoting

- Bare words split on unquoted whitespace.
- `'...'` — single-quoted: literal, no expansion, no escape processing.
- `"..."` — double-quoted: variable expansion still happens, `\\` and
  `\"` and `\$` are honoured, everything else is literal.
- `\X` outside quotes — escape any single character.

### Variable expansion

- `$NAME` and `${NAME}` — substitute a shell or environment variable.
  Unset variables expand to the empty string.
- `$0`..`$9`, `$#`, `$?` — only inside script execution
  (`shell_script.c`); `$0` is the script path, `$#` is the positional
  argument count, `$?` is the exit code of the last command.
- Assignments of the form `NAME=value` set a shell variable for the
  current shell; combine with `export NAME` (or `export NAME=value`)
  to make it visible to spawned children.

There is **no** command substitution (`$(cmd)` / backticks) and **no**
arithmetic expansion in v2.0 — both are listed as future work in the
plan's §2 non-goals.

### Operators

| Operator | Meaning |
|----------|---------|
| `;`  | Sequence — run left, then right, regardless of exit status |
| `&&` | Run right only if left exited 0 |
| `||` | Run right only if left exited non-zero |
| `|`  | Pipe — implemented as `a >/tmp/p.N ; b </tmp/p.N` (no concurrency) |
| `<`  | Redirect stdin from the named file |
| `>`  | Redirect stdout to the named file (truncate / create) |
| `>>` | Redirect stdout to the named file (append / create) |

Pipes do *not* run concurrently — the left side runs to completion,
buffering its stdout into a temp file under `/tmp/`, then the right
side runs with that temp file wired to its stdin. This keeps the
single-thread execution model intact while letting users compose tools
like `cat foo | grep bar > out`.

## Built-ins

The built-in registry lives in `shell_exec.c` as a static
`{ name, function }` table. Adding a built-in is a one-line edit there
plus the implementation in `shell_cmds.c`.

| Built-in | Notes |
|----------|-------|
| `help` | Prints a short list of built-ins |
| `clear` | `\x1b[2J\x1b[H` to fd 1 |
| `echo <text>...` | Prints args separated by spaces, then `\n` |
| `uptime` | Seconds since boot |
| `pwd` | Print working directory |
| `cd [path]` | Change directory; bare `cd` goes to `/` |
| `ls [path]` | Directory listing with size column |
| `mkdir <path>` / `mkfile <path>` / `rm <path>` | Tree mutation |
| `cat <file>` / `write <file> <text>` | File read / overwrite |
| `cp <src> <dst>` / `mv <src> <dst>` | Copy / rename |
| `sync` | Flush BRFS write cache to flash |
| `df` | Filesystem usage |
| `jobs` / `fg <pid>` / `kill <pid>` | Job control |
| `export NAME[=val]` / `set NAME[=val]` / `unset NAME` / `env` | Variables |
| `exit [code]` | Exit the shell (only meaningful in scripts / nested shells) |
| `true` / `false` | Always exit 0 / 1 — useful in chains |

The previous `format` built-in was moved out to `/bin/format` in Phase E
(use it as `format <blocks> <bytes-per-block> <label>`). The kernel
still owns the boot-time mount-failure prompt because that path runs
before any external binary is reachable.

## Program lookup

Anything that is not a built-in is treated as a program reference:

1. If the token contains `/` or starts with `.`, it is treated as a
   path — absolute (`/bin/foo`) or relative to the current working
   directory (`./foo`, `subdir/foo`).
2. Otherwise the shell tries `/bin/<name>` first, then falls back to
   `<cwd>/<name>`.

Argument vectors are copied into a per-process arena (see the process
model section in [OS.md](OS.md#process-model)) so the child cannot
corrupt the shell's input buffers.

## Scripts

A file whose first line is `#!/bin/sh` (or which is invoked as
`sh script.sh`) is interpreted by `shell_script.c`. Inside a script
the following extras are available:

- `$0` is the script path; `$1`..`$9` are the positional arguments;
  `$#` is the count of positionals; `$?` is the exit status of the
  most recent command.
- `set -e` aborts the script the moment any command exits non-zero.
- Comments starting with `#` are stripped per line.

There is no flow control (`if`/`then`/`fi`, loops, functions) yet —
those are listed in the plan's §2 as deferred features.

## See also

- [OS.md — Process model & VFS](OS.md#process-model)
- [Terminal.md — libterm v2 + supported ANSI escapes](Terminal.md)
- [shell-terminal-v2 plan](../../plans/shell-terminal-v2.md)
