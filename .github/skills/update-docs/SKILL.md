---
name: update-docs
description: 'Update documentation after making code or hardware changes. Use when asked to update docs, write documentation, or after completing a feature that needs documenting.'
---
# Update documentation

## Documentation structure
| Path | Purpose |
|------|---------|
| `Docs/docs/` | MkDocs website (user-facing documentation) |
| `Docs/context/` | AI agent context files (deep technical reference) |
| `Docs/plans/` | Development plans and decision logs, these are not checked into git and are only for communicating with the user on a specific feature or task |
| `.github/copilot-instructions.md` | AI agent global instructions |
| `.github/instructions/` | AI agent path-specific instructions |

## Which docs to update

### After a hardware change (Verilog)
1. `Docs/docs/Hardware/` — relevant hardware doc page
2. `Docs/context/Project-context.md` — if MMIO registers, interrupts, or SPI assignments changed

### After a kernel change (BDOS)
1. `Docs/context/BDOS-context.md` — if syscalls, memory map, interrupts, or boot sequence changed
2. `.github/instructions/bdos-kernel.instructions.md` — if file map or syscall table changed

### After a driver change (libfpgc)
1. `Docs/docs/Hardware/IO/` — relevant I/O doc page
2. `Docs/context/Project-context.md` — if peripheral behavior changed
3. `.github/instructions/libfpgc-drivers.instructions.md` — if API changed

### After a new userBDOS program
1. `Docs/docs/Software/` — add program documentation
2. `.github/instructions/userbdos.instructions.md` — add to program list

### After a toolchain change
1. `Docs/docs/Development/` — toolchain documentation
2. `.github/instructions/toolchain.instructions.md` — if build commands changed

## Build and preview docs
```
make docs-serve     # Run documentation website locally
make docs-deploy    # Deploy documentation website
```

## Plan documents
When completing a feature from a plan in `Docs/plans/`:
1. Add `Status: COMPLETED` at the top of the plan file
2. Add a one-line summary of the outcome
3. List all files that were changed

## AI context files
The files in `Docs/context/` are comprehensive technical references
for AI agents. When updating them:
- Keep factual and precise — these are reference docs, not tutorials
- Include exact file paths, function names, register addresses
- Update tables rather than prose when possible
- Verify facts against the actual source code
