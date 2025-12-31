# Address Decoder

The B32P3 uses distributed address decoding logic within the main CPU module rather than a separate `AddressDecoder` module. This approach reduces latency and simplifies the design by embedding address decoding directly in the pipeline stages where it's needed.

## Memory Map

The CPU implements a 32-bit memory map with the following regions:

| Start Address | End Address | Memory Type | Description |
|---------------|-------------|-------------|-------------|
| `0x0000000` | `0x6FFFFFF` | SDRAM | Main system memory (cached) |
| `0x7000000` | `0x77FFFFF` | I/O | Memory-mapped peripherals |
| `0x7800000` | `0x78FFFFF` | ROM | Boot ROM |
| `0x7900000` | `0x79FFFFF` | VRAM32 | 32-bit video memory |
| `0x7A00000` | `0x7AFFFFF` | VRAM8 | 8-bit video memory |
| `0x7B00000` | `0x7BFFFFF` | VRAMPX | Pixel buffer |

## Address Calculation

Memory addresses are calculated by adding a base register and signed 16-bit offset:

```verilog
wire [31:0] ex_mem_addr_calc = ex_alu_a + id_ex_const16;
```

This supports common addressing patterns:

- **Array access**: `base + index * element_size`
- **Structure field access**: `struct_ptr + field_offset`
- **Stack-relative**: `SP + local_var_offset`

## Decode Logic

Address decoding is performed in the EX and MEM stages:

### EX Stage Decoding (for BRAM setup)

BRAM-based memories (ROM, VRAM) require addresses one cycle early due to their read latency:

```verilog
wire ex_sel_rom    = ex_mem_addr_calc >= 32'h7800000 && ex_mem_addr_calc < 32'h7900000;
wire ex_sel_vram32 = ex_mem_addr_calc >= 32'h7900000 && ex_mem_addr_calc < 32'h7A00000;
wire ex_sel_vram8  = ex_mem_addr_calc >= 32'h7A00000 && ex_mem_addr_calc < 32'h7B00000;
wire ex_sel_vrampx = ex_mem_addr_calc >= 32'h7B00000 && ex_mem_addr_calc < 32'h7C00000;
```

### MEM Stage Decoding (for data selection)

Data source selection happens in MEM stage when read data is available:

```verilog
wire mem_sel_sdram  = ex_mem_mem_addr >= 32'h0000000 && ex_mem_mem_addr < 32'h7000000;
wire mem_sel_io     = ex_mem_mem_addr >= 32'h7000000 && ex_mem_mem_addr < 32'h7800000;
wire mem_sel_rom    = ex_mem_mem_addr >= 32'h7800000 && ex_mem_mem_addr < 32'h7900000;
wire mem_sel_vram32 = ex_mem_mem_addr >= 32'h7900000 && ex_mem_mem_addr < 32'h7A00000;
wire mem_sel_vram8  = ex_mem_mem_addr >= 32'h7A00000 && ex_mem_mem_addr < 32'h7B00000;
wire mem_sel_vrampx = ex_mem_mem_addr >= 32'h7B00000 && ex_mem_mem_addr < 32'h7C00000;
```

## Local Address Calculation

Each memory region uses local addresses starting from 0:

```verilog
wire [31:0] ex_local_addr_rom    = ex_mem_addr_calc - 32'h7800000;
wire [31:0] ex_local_addr_vram32 = ex_mem_addr_calc - 32'h7900000;
wire [31:0] ex_local_addr_vram8  = ex_mem_addr_calc - 32'h7A00000;
wire [31:0] ex_local_addr_vrampx = ex_mem_addr_calc - 32'h7B00000;
```

## Memory Type Characteristics

### SDRAM

- Accessed via L1D cache
- Cache miss triggers cache controller request
- Writes go through cache controller

### ROM

- Read-only memory
- Dual-port: instruction fetch + data access
- 1-cycle read latency (BRAM)

### VRAM (32, 8, PX)

- Single-cycle read/write (BRAM)
- Different data widths: 32-bit, 8-bit, 8-bit
- Address setup in EX, data available in MEM

### I/O

- Accessed via Memory Unit
- Variable latency depending on peripheral
- Full address passed for device selection
