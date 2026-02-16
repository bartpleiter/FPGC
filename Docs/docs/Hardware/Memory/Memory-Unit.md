# Memory Unit

The Memory Unit (MU) serves as a unified interface between the CPU and slow memory or I/O devices. It acts as a bridge for accessing various peripheral devices through a standardized memory-mapped interface, allowing the CPU to communicate with I/O devices using simple read and write operations.

## Overview

The Memory Unit is designed with the following principles:

- **Simplicity over speed**: Optimized for ease of implementation rather than high performance
- **Unified interface**: Presents various I/O devices to the CPU through a single, consistent bus interface
- **State-machine based**: Uses internal state machines to handle multi-cycle operations with different peripheral devices
- **Memory-mapped I/O**: All devices appear as memory locations to the CPU, simplifying software access

High-performance memory types (SDRAM, ROM, VRAM) bypass the Memory Unit and connect directly to the CPU to avoid unnecessary latency. This design choice makes the Memory Unit primarily focused on I/O operations and lower-speed peripherals.

## Module Declaration

```verilog
module MemoryUnit(
    // System interface
    input  wire         clk,        // System clock (50MHz)
    input  wire         reset,      // Reset signal

    // CPU interface
    input  wire         start,      // Start operation
    input  wire [31:0]  addr,       // Address in CPU words
    input  wire [31:0]  data,       // Write data
    input  wire         we,         // Write enable
    output reg  [31:0]  q,          // Read data
    output reg          done,       // Operation complete

    // I/O signals
    // E.g. HW signals and interrupts
);
```

## Architecture Overview

The Memory Unit implements a simple request-response protocol with the CPU and manages several I/O devices through dedicated controllers:

```text
┌─────────┐    ┌──────────────┐    ┌─────────────┐    ┌─────────────┐
│   CPU   │◄──►│ Memory Unit  │◄──►│ UART TX/RX  │◄──►│ HW signals  │
│         │    │              │    └─────────────┘    └─────────────┘
│         │    │ State Machine│    ┌─────────────┐
│         │    │   Control    │◄──►│   OS Timer  │
│         │    │              │    └─────────────┘
└─────────┘    └──────────────┘    ┌─────────────┐
                                   │ Other IO    │
                                   │ Controllers │
                                   └─────────────┘
```

## I/O Device Integration

Each I/O device is managed by a dedicated controller module that handles the specifics of communication with that device. The Memory Unit's state machine coordinates these controllers based on the CPU's requests.

## Memory Map

The Memory Unit implements the I/O portion of the CPU memory map starting at address `0x7000000`.

!!! info "Related Documentation"
    See [Memory Map](Memory-Map.md) for the complete FPGC memory layout and addressing scheme.

!!! note "Write-Only Assumption"
    The current implementation assumes write operations for most addresses to prevent CPU lockup. Read operations on write-only addresses (e.g. UART TX and User LED) will still complete but shall probably return 0.

## State Machine Operation

The Memory Unit uses a finite state machine to handle different I/O operations:

### Operation Flow

1. **Idle State**: Monitor for `start` signal from CPU
2. **Address Decode**: Determine target device based on address
3. **Device Operation**: Execute appropriate read/write operation
4. **Wait/Complete**: For multi-cycle operations, wait for completion
5. **Response**: Assert `done` signal and provide result data

## CPU Interface Protocol

### Write Operation

```text
Cycle 1: CPU asserts start=1, addr=target, data=write_data, we=1
Cycle 2: MU begins operation, may enter wait state
Cycle N: MU asserts done=1, CPU can proceed
```

### Read Operation

```text
Cycle 1: CPU asserts start=1, addr=target, we=0
Cycle 2: MU begins operation, may enter wait state
Cycle N: MU asserts done=1, q=read_data, CPU reads result
```
