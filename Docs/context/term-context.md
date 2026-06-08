# FPGC Terminal Color & Palette Reference

## Architecture

The FPGC terminal (`Software/C/libfpgc/term/term.c`) uses a **single 8-bit palette index per cell** (0-255), not separate foreground/background RGB values. Each cell stores a `tile` (character) and a `palette` (unsigned char). The GPU maps each palette index to an actual color.

## GPU Default Palette Indices

| Index | Appearance | Notes |
|-------|-----------|-------|
| 0 | white-on-black | Default, used by `\x1b[0m` reset |
| 1 | black-on-white | Inverted, used for "ANSI black" fg |
| 2 | red | |
| 3 | green | |
| 4 | blue | |
| 5 | yellow | |
| 6 | magenta | |
| 7 | cyan | |
| 8-255 | GPU-defined | Direct palette access via 256-color escapes |

Palette 235 renders as dark gray. Palette 254 renders as white.

## ANSI Escape Handling

### Supported SGR codes

| Escape | Effect |
|--------|--------|
| `\x1b[0m` | Reset palette to 0 (white-on-black) |
| `\x1b[1m` | Bold: `palette \|= 0x08` (bump fg by +8) |
| `\x1b[30m` | Palette = `k_ansi_fg[0]` = **1** (black-on-white) |
| `\x1b[31m` | Palette = `k_ansi_fg[1]` = **2** (red) |
| `\x1b[32m` | Palette = `k_ansi_fg[2]` = **3** (green) |
| `\x1b[33m` | Palette = `k_ansi_fg[3]` = **5** (yellow) |
| `\x1b[34m` | Palette = `k_ansi_fg[4]` = **4** (blue) |
| `\x1b[35m` | Palette = `k_ansi_fg[5]` = **6** (magenta) |
| `\x1b[36m` | Palette = `k_ansi_fg[6]` = **7** (cyan) |
| `\x1b[37m` | Palette = `k_ansi_fg[7]` = **0** (white-on-black) |
| `\x1b[40m` | `palette = (palette & 0x0F) \| (0 << 4)` — sets high nibble to 0 |
| `\x1b[41m`-`\x1b[47m` | Sets high nibble to 1-7 (modifies existing palette) |
| `\x1b[38;5;Nm` | Palette = **N** (direct 0-255, full override) |
| `\x1b[48;5;Nm` | Palette = **N** (same as 38;5;N, full override) |

### NOT supported (silently ignored)

- SGR 90-97 (bright foregrounds: `\x1b[90m` through `\x1b[97m`)
- SGR 100-107 (bright backgrounds)
- Any other SGR codes not listed above

**Important:** Because SGR 90-97 are ignored, they can be safely used as "no-op" foreground escapes that won't override a palette set by a 256-color bg escape.

## Critical Gotcha

**Standard ANSI fg codes (30-37) OVERRIDE the entire palette.** They do NOT just set the foreground nibble. So this sequence does NOT work as expected:

```
\x1b[48;5;235m   → palette = 235 (dark gray)
\x1b[30m         → palette = 1 (OVERRIDES to black-on-white!)
```

The `\x1b[30m` clobbers the palette set by the 256-color escape.

## Correct Pattern for UserBDOS Programs

To set a specific palette, use `\x1b[48;5;Nm` (or `\x1b[38;5;Nm`) and then use an unsupported fg code (like `\x1b[90m`) as a no-op:

```c
static const char *tile_bg_colors[] = {
    [0] = "\x1b[48;5;235m",  /* palette 235 = dark gray (empty cells) */
    [1] = "\x1b[48;5;1m",    /* palette 1 = black-on-white (numbered cells) */
};

static const char *tile_fg_colors[] = {
    [0] = "\x1b[90m",  /* SGR 90 = ignored, palette stays at bg value */
    [1] = "\x1b[90m",  /* same */
};
```

Render each cell as: `write_str(bg_color); write_str(fg_color); write_content(); reset_colors();`

Where `reset_colors()` writes `\x1b[0m` to restore palette 0 for borders/separators.

## Background-Only Escapes (40-47)

`\x1b[40m` through `\x1b[47m` modify only the high nibble: `palette = (palette & 0x0F) | ((v-40) << 4)`. These are useful for combining with an existing low nibble, but NOT for setting a specific palette index. They are NOT equivalent to 256-color escapes.

## Source

`Software/C/libfpgc/term/term.c`, SGR handler around line 487-527.
