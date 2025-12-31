# Architecture Diagrams

This document provides a visual representation of the B32P3 CPU architecture, showing the pipeline stages, data flow, and module relationships.

## Complete CPU Architecture

```text
                                 B32P3 CPU Architecture
                                  (5-Stage Pipeline)

┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    HAZARD CONTROL                                       │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                                      │
│  │flush_if_id  │  │flush_id_ex  │  │flush_ex_mem │                                      │
│  │stall_if     │  │stall_id     │  │             │                                      │
│  └─────────────┘  └─────────────┘  └─────────────┘                                      │
└─────────────────────────────────────────────────────────────────────────────────────────┘
        │                 │                 │
        ▼                 ▼                 ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌────────────┐
│     IF      │───▶│     ID      │───▶│     EX      │───▶│    MEM      │───▶│     WB     │
│ Instruction │    │ Decode &    │    │  Execute    │    │   Memory    │    │ Writeback  │
│   Fetch     │    │ Reg Read    │    │    ALU      │    │   Access    │    │            │
│             │    │             │    │             │    │   Branch    │    │            │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘    └────────────┘
       │                  │                 │                  │                  │
       │                  │                 │                  │                  │
       ▼                  ▼                 ▼                  ▼                  ▼
  ┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
  │L1I Cache│       │Instr    │       │  ALU    │       │L1D Cache│       │Regbank  │
  │  ROM    │       │Decoder  │       │MultiALU │       │  Stack  │       │  Write  │
  │         │       │Regbank  │       │Forward  │       │ Branch  │       │         │
  │         │       │(addr)   │       │         │       │ Jump    │       │         │
  └─────────┘       └─────────┘       └─────────┘       └─────────┘       └─────────┘

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    FORWARDING NETWORK                                                    │
│                                                                                                          │
│  EX/MEM → EX: ALU results (not loads/pops - data not ready yet)                                          │
│  MEM/WB → EX: All writeback data (ALU results, memory data, stack data)                                  │
│                                                                                                          │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                  EXTERNAL INTERFACES                                                     │
│                                                                                                          │
│  ROM (Dual Port): Instruction fetch + Data access                                                        │
│  L1I Cache: Instruction cache with tag/valid check                                                       │
│  L1D Cache: Data cache with tag/valid check                                                              │
│  Video Memory: VRAM32, VRAM8, VRAMPX for GPU communication                                               │
│  Cache Controller: 2x SDRAM interface via SDRAM Controller                                               │
│  Memory Unit: I/O device access                                                                          │
│                                                                                                          │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Pipeline Data Flow

```text
                              Pipeline Registers & Data Flow
                                        
    IF/ID                   ID/EX                   EX/MEM                  MEM/WB
  ┌────────┐             ┌────────┐             ┌────────┐             ┌────────┐
  │  pc    │────────────▶│  pc    │────────────▶│  pc    │────────────▶│  pc    │
  │ instr  │────────────▶│ instr  │────────────▶│ instr  │────────────▶│ instr  │
  │ valid  │────────────▶│ valid  │────────────▶│ valid  │────────────▶│ valid  │
  └────────┘             │        │             │        │             │        │
                         │ dreg   │────────────▶│ dreg   │────────────▶│ dreg   │
                         │ areg   │             │alu_res │────────────▶│alu_res │
                         │ breg   │             │breg_dat│             │mem_data│
                         │        │             │mem_addr│             │stk_data│
                         │alu_op │             │        │             │        │
                         │const16│────────────▶│const16 │             │ result │
                         │const27│────────────▶│const27 │             │        │
                         │        │             │        │             │        │
                         │control│────────────▶│control │────────────▶│control │
                         │ flags │             │ flags  │             │ flags  │
                         └────────┘             └────────┘             └────────┘
```

## Hazard Detection Logic

```text
                     ┌─────────────────────────────────────┐
                     │          HAZARD DETECTOR            │
                     └─────────────────────────────────────┘
                                       │
        ┌──────────────────────────────┼──────────────────────────────┐
        │                              │                              │
        ▼                              ▼                              ▼
┌─────────────────┐            ┌─────────────────┐           ┌─────────────────┐
│    DATA         │            │    CONTROL      │           │    STRUCTURAL   │
│   HAZARDS       │            │    HAZARDS      │           │    HAZARDS      │
└─────────────────┘            └─────────────────┘           └─────────────────┘
        │                              │                              │
        ▼                              ▼                              ▼
┌─────────────────┐            ┌─────────────────┐           ┌─────────────────┐
│ • Load-use      │            │ • Branch taken  │           │ • Cache miss    │
│ • Pop-use       │            │ • Jump          │           │ • Multi-cycle   │
│ • Cache line    │            │ • Interrupt     │           │   ALU           │
│   conflict      │            │ • RETI          │           │ • Memory Unit   │
└─────────────────┘            └─────────────────┘           └─────────────────┘
        │                              │                              │
        ▼                              ▼                              ▼
┌─────────────────┐            ┌─────────────────┐           ┌─────────────────┐
│   STALL IF/ID   │            │  FLUSH IF/ID    │           │  STALL ENTIRE   │
│                 │            │  FLUSH ID/EX    │           │  PIPELINE       │
│                 │            │  FLUSH EX/MEM   │           │  (backend_stall)│
└─────────────────┘            └─────────────────┘           └─────────────────┘
```

## Forwarding Network Detail

```text
                         Forwarding to EX Stage ALU Inputs
                         
                    ┌─────────────────────────────────────────────┐
                    │              EX Stage                       │
                    │                                             │
  ┌──────────┐      │   ┌──────────┐          ┌──────────┐       │
  │ Regbank  │─────▶│──▶│  MUX A   │─────────▶│          │       │
  │ data_a   │      │   │  00:reg  │          │          │       │
  └──────────┘      │   │  01:EX/M │          │   ALU    │──────▶│───▶ To EX/MEM
                    │   │  10:M/WB │          │          │       │
  ┌──────────┐      │   └──────────┘          │          │       │
  │ EX/MEM   │─────▶│────────┘                │          │       │
  │ alu_res  │      │                         └──────────┘       │
  └──────────┘      │   ┌──────────┐               ▲             │
                    │   │  MUX B   │               │             │
  ┌──────────┐      │   │  00:reg  │               │             │
  │ MEM/WB   │─────▶│──▶│  01:EX/M │───┬───────────┘             │
  │ result   │      │   │  10:M/WB │   │                         │
  └──────────┘      │   └──────────┘   │     ┌──────────┐        │
                    │                  │     │   MUX    │        │
  ┌──────────┐      │                  └────▶│ reg/const│───────▶│
  │ ID/EX    │      │                        │          │        │
  │ const    │─────▶│───────────────────────▶│          │        │
  └──────────┘      │                        └──────────┘        │
                    └─────────────────────────────────────────────┘
                    
  Forward Conditions:
  ─────────────────────────────────────────────────────────────────
  EX/MEM → EX (01): ex_mem_dreg == id_ex_areg/breg, dreg_we=1,
                    dreg!=0, NOT mem_read, NOT pop
  MEM/WB → EX (10): mem_wb_dreg == id_ex_areg/breg, dreg_we=1,
                    dreg!=0
```

## Cache Architecture Integration

```text
                L1 Instruction Cache                           L1 Data Cache
    ┌─────────────────────────────────────────┐       ┌─────────────────────────────────────────┐
    │            CPU Pipeline                 │       │            CPU Pipeline                 │
    │  ┌─────────┐                            │       │  ┌─────────┐         ┌─────────┐        │
    │  │    IF   │ (Cache address setup)      │       │  │   EX    │────────▶│   MEM   │        │
    │  └─────────┘                            │       │  └─────────┘         └─────────┘        │
    │       │                                 │       │       │                   │             │
    │       ▼                                 │       │       ▼                   ▼             │
    │  ┌─────────┐                            │       │  ┌─────────┐         ┌─────────┐        │
    │  │L1I Cache│ (1-cycle BRAM read)        │       │  │L1D Cache│         │ Hit/Miss│        │
    │  │ Access  │                            │       │  │ Access  │         │ Detect  │        │
    │  └─────────┘                            │       │  └─────────┘         └─────────┘        │
    │       │                                 │       │                           │             │
    │       ▼                                 │       │                           │             │
    │  ┌─────────┐                            │       │                           │             │
    │  │ Hit/Miss│ (Tag compare, stall)       │       │                           │             │
    │  │ Detect  │                            │       │                           │             │
    │  └─────────┘                            │       │                           │             │
    └─────────────────────────────────────────┘       └─────────────────────────────────────────┘
                      │                                                 │
                      ▼                                                 ▼
    ┌───────────────────────────────────────────────────────────────────────────────────────────┐
    │                                     Cache Controller                                      │
    │                                                                                           │
    │  ┌─────────┐    ┌─────────┐    ┌──────┐            ┌─────────┐    ┌─────────┐    ┌──────┐ │
    │  │Miss     │    │ SDRAM   │    │Cache │            │Miss     │    │ SDRAM   │    │Cache │ │
    │  │Handling │───▶│Interface│───▶│Update│            │Handling │───▶│Interface│───▶│Update│ │
    │  └─────────┘    └─────────┘    └──────┘            └─────────┘    └─────────┘    └──────┘ │
    └───────────────────────────────────────────────────────────────────────────────────────────┘
                                                 │
                                                 ▼
    ┌─────────────────────────────────────────────────────────────────────────────────────────┐
    │                                    SDRAM Controller                                     │
    │                Simple SDRAM controller for single burst reads and writes                │
    │                                                                                         │
    └─────────────────────────────────────────────────────────────────────────────────────────┘
                                                 │
                                                 ▼
                                           ┌─────────────┐
                                           │   2x16bit   │
                                           │   SDRAM     │
                                           │   Memory    │
                                           └─────────────┘
```

## Branch Resolution (MEM Stage)

```text
                    Branch Resolution in MEM Stage
                    
    ┌────────────────────────────────────────────────────────────┐
    │                      MEM Stage                             │
    │                                                            │
    │  ┌──────────┐    ┌────────────────┐    ┌──────────────┐   │
    │  │ EX/MEM   │───▶│ Pre-computed   │───▶│ Branch/Jump  │   │
    │  │ areg_dat │    │ addresses      │    │    Unit      │   │
    │  │ breg_dat │    │                │    │              │   │
    │  │ branch_op│    │ • branch_addr  │    │ Evaluates:   │   │
    │  │ const16  │    │ • jump_addr    │    │ • BEQ/BNE    │   │
    │  │ const27  │    │                │    │ • BGT/BLT    │   │
    │  │ pc       │    │                │    │ • BGE/BLE    │   │
    │  └──────────┘    └────────────────┘    │ • JUMP/JUMPR │   │
    │                                        │ • HALT       │   │
    │                                        └──────────────┘   │
    │                                               │           │
    │                                               ▼           │
    │                                        ┌──────────────┐   │
    │                                        │ jump_valid   │   │
    │                                        │ jump_addr    │   │
    │                                        └──────────────┘   │
    └────────────────────────────────────────────────────────────┘
                              │
                              ▼
    ┌────────────────────────────────────────────────────────────┐
    │                   PC Redirect Logic                        │
    │                                                            │
    │  pc_redirect = ex_mem_valid && jump_valid                  │
    │  pc_redirect_target = jump_addr                            │
    │                                                            │
    │  On redirect:                                              │
    │    • Flush IF/ID, ID/EX, EX/MEM registers                  │
    │    • Update PC to jump_addr                                │
    │    • 2-cycle branch penalty                                │
    └────────────────────────────────────────────────────────────┘
```
