# Shell

The BDOS shell (`/bin/sh`) is a Bourne-style interactive shell implemented as a userland program (`Software/C/userBDOS/sh.c`). It is spawned by `/bin/init` (PID 1) and respawned automatically when it exits.

```bash
make compile-userbdos file=sh   # Compile the shell
```

## Syntax

The grammar follows traditional Bourne shell conventions:

```
LINE      := CHAIN ( ';' CHAIN )*
CHAIN     := PIPELINE ( '&&' PIPELINE | '||' PIPELINE )*
PIPELINE  := COMMAND ( '|' COMMAND )*
COMMAND   := WORD+ REDIR*
REDIR     := '<' FILE | '>' FILE | '>>' FILE
```

Comments start with `#` and extend to end of line.

## Words and Quoting

| Syntax | Behaviour |
|--------|-----------|
| `bare` | Split on whitespace; glob and variable expansion applied |
| `'single'` | Literal — no expansion, no escapes |
| `"double"` | Variable expansion (`$`) applied; `\"`, `\\`, `\$` recognized |
| `\X` | Escape any single character (outside quotes) |

## Variables

Shell variables are set with `NAME=value` and accessed with `$NAME` or `${NAME}`.

| Variable | Meaning |
|----------|---------|
| `$NAME` / `${NAME}` | Shell or environment variable (empty string if unset) |
| `$?` | Exit code of the last command |
| `$$` | Current process PID |
| `$#` | Positional argument count (scripts only) |
| `$0`–`$9` | Positional parameters (scripts only) |

Default environment variables: `PATH=/bin`, `HOME=/`.

Use `export NAME[=value]` to make a variable visible to child processes.

## Operators

| Operator | Description |
|----------|-------------|
| `;` | Sequence — run left, then right regardless of exit code |
| `&&` | AND — run right only if left exits 0 |
| `\|\|` | OR — run right only if left exits non-zero |
| `\|` | Pipe — connect stdout of left to stdin of right |
| `<` | Redirect stdin from file |
| `>` | Redirect stdout to file (truncate) |
| `>>` | Redirect stdout to file (append) |

### Pipes

Pipes are implemented via temporary files (`/tmp/p.0`, `/tmp/p.1`, ...) rather than concurrent processes. In a pipeline `a | b`:

1. `a` runs with stdout redirected to `/tmp/p.0`
2. `b` runs with stdin redirected from `/tmp/p.0`
3. The temp file is cleaned up automatically

## Control Flow

### if / elif / else / fi

```shell
if test -f /bin/hello
then
    echo "hello exists"
elif test -f /bin/hi
then
    echo "hi exists"
else
    echo "neither found"
fi
```

The condition is evaluated as a command — exit code 0 means true, non-zero means false. Nesting is supported.

### for / in / do / done

```shell
for f in *.txt
do
    echo "File: $f"
done
```

The word list is expanded (including globs) before iteration.

### while / do / done

```shell
while test -f /tmp/lock
do
    sleep 1
done
```

A safety limit of 10,000 iterations prevents infinite loops.

## Test Expressions

The `test` built-in (also available as `[`) evaluates conditionals:

### Unary

| Expression | True when |
|------------|-----------|
| `STRING` | String is non-empty |
| `-n STRING` | String is non-empty |
| `-z STRING` | String is empty |
| `-f PATH` | File exists |
| `-d PATH` | Directory exists |
| `! EXPR` | Expression is false |

### Binary (string)

| Expression | True when |
|------------|-----------|
| `S1 = S2` | Strings are equal |
| `S1 != S2` | Strings differ |

### Binary (integer)

| Expression | True when |
|------------|-----------|
| `N1 -eq N2` | Equal |
| `N1 -ne N2` | Not equal |
| `N1 -lt N2` | Less than |
| `N1 -gt N2` | Greater than |

## Glob Expansion

Patterns are expanded after tokenization:

| Pattern | Matches |
|---------|---------|
| `*` | Any sequence of characters |
| `?` | Any single character |
| `[abc]` | Any character in set |
| `[a-z]` | Any character in range |
| `[!a-z]` | Any character NOT in range |

Hidden files (starting with `.`) are skipped unless the pattern itself starts with `.`. If no files match, the pattern is returned as-is.

## Built-in Commands

| Command | Description |
|---------|-------------|
| `help` | List built-ins and available programs in `/bin/` |
| `clear` | Clear the terminal screen |
| `echo <text>` | Print text to stdout |
| `cd [path]` | Change directory (bare `cd` goes to `/`) |
| `pwd` | Print working directory |
| `export [NAME[=VAL]]` | Export variable or list all exported variables |
| `set` | List all shell variables |
| `unset NAME` | Remove a variable |
| `env` | List exported variables |
| `test EXPR` / `[ EXPR ]` | Evaluate a conditional expression |
| `true` | Exit with code 0 |
| `false` | Exit with code 1 |
| `history` | Show numbered command history |
| `exit [N]` | Exit the shell with optional exit code |
| `halt` | Halt the system |

## External Commands

Anything that is not a built-in is resolved as a program:

1. Bare names: try `/bin/<name>` first, then the current directory
2. Names containing `/` or starting with `.`: resolved as paths (absolute or relative)

Programs are spawned as child processes via `SYS_SPAWN` and the shell waits for them to exit via `SYS_WAITPID`.

## Script Execution

Scripts start with a `#!/bin/sh` shebang line. To execute:

```bash
sh myscript.sh arg1 arg2
```

Within a script, positional parameters `$0`–`$9` and `$#` are available. Scripts abort on the first non-zero exit code (implicit `set -e`). Maximum script size is 8192 bytes / 128 lines. Nested script execution is not supported.

## Interactive Features

### Command History

The shell maintains a 32-entry command history (ring buffer). Navigate with **Up/Down** arrow keys. Duplicate entries are suppressed. Use the `history` built-in to display the numbered list.

### Tab Completion

- **Command position**: completes built-in names, control flow keywords, and programs in `/bin/`
- **Argument position**: completes filenames in the current or specified directory
- **Single match**: auto-completes in place
- **Multiple matches**: fills the longest common prefix and lists all candidates

### Line Editing

| Key | Action |
|-----|--------|
| Left / Right | Move cursor |
| Home / Ctrl-A | Move to start of line |
| End / Ctrl-E | Move to end of line |
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |
| Up / Down | Navigate command history |
| Tab | Auto-complete |
