---
name: 'Verilog RTL'
description: 'Rules for editing FPGA hardware design files'
applyTo: 'Hardware/**/*.v'
---
# Verilog RTL guidelines

## Validation
After editing any Verilog file, run:
```
make test-cpu          # Run all CPU tests (parallel)
make quartus-timing    # Check timing closure
```

For a single test: `make test-cpu-single file=<test_file>`
Debug with GTKWave: `make debug-cpu file=<test_file>`

## Key modules
| Module | Path | Purpose |
|--------|------|---------|
| CPU pipeline | `Hardware/FPGA/Verilog/Modules/CPU/` | B32P3 five-stage pipeline |
| DMA engine | `Hardware/FPGA/Verilog/Modules/IO/DMAengine.v` | 7 DMA modes |
| SPI controllers | `Hardware/FPGA/Verilog/Modules/IO/SimpleSPI.v` | SPI bus 0-5 |
| Timer | `Hardware/FPGA/Verilog/Modules/IO/OStimer.v` | 3 hardware timers |
| UART | `Hardware/FPGA/Verilog/Modules/IO/UARTtx.v`, `UARTrx.v` | Serial port |
| GPU | `Hardware/FPGA/Verilog/Modules/GPU/` | 320×240 tile + pixel engine |
| SDRAM controller | `Hardware/FPGA/Verilog/Modules/Memory/` | 64 MiB SDRAM |
| Top-level | `Hardware/FPGA/CycloneIV_EP4CE40/FPGC.v` | MMIO address decoder, module instantiation |

## Conventions
- All MMIO registers are at addresses `0x1C000000`–`0x1C000084`
- New MMIO registers must be added to the address decoder in `FPGC.v`
  AND to `Software/C/libfpgc/include/fpgc.h`
- Clock domain: single 50 MHz clock (no CDC unless interfacing external async signals)

## Ripple effects
- Adding a new MMIO register → also update `fpgc.h`, `Docs/context/Project-context.md`
- Changing interrupt IDs → also update `main.c` interrupt dispatcher
- Changing SPI bus assignments → also update `fpgc.h` SPI defines

## Reference implementation
To add a new I/O peripheral, study `Hardware/FPGA/Verilog/Modules/IO/DMAengine.v`
as the reference — it shows register interfacing, interrupt generation, and
MMIO integration.
