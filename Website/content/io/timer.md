---
title: Timer and cycle counter
weight: 2
reader: programming
---

Two devices for time, both counting in clock cycles of 10 ns: the cycle counter tells how much time went by, and the timer gives an interrupt after a number of cycles.

## Cycle counter

A 64-bit counter that counts every cycle since the last reset. At 100 MHz it would take more than 5,000 years to wrap around.

| Address | Register | Read | Write |
| --- | --- | --- | --- |
| `0xFFFF8100` | SNAPSHOT | 0 | any value: copies the count into the snapshot |
| `0xFFFF8104` | LOW | bits [31:0] of the snapshot | ignored |
| `0xFFFF8108` | HIGH | bits [63:32] of the snapshot | ignored |

The count changes every cycle, and a program reads it in two halves. If it read the halves of the running count, the low half could wrap around between the two reads, and the two halves wouldn't belong together. The snapshot holds still, so both halves are from the same moment.

### Measuring code

Take a snapshot before and after the code, and subtract the low words:

```text
    write 0xFFFF8100 r0 r0      ; snapshot
    read 0xFFFF8104 r0 r1       ; its low word
    ...                         ; the code to measure
    write 0xFFFF8100 r0 r0
    read 0xFFFF8104 r0 r2
    sub r2 r1 r3                ; the cycles
```

- With no code in between, the difference is 9 cycles, which the snapshot accesses take themselves. Subtract that to get the cycles of the code.
- The low word alone is enough for anything shorter than 42 seconds.
- Code in SDRAM can take longer the first time it runs, when it's not in the caches yet. Measuring it twice in a row shows the difference.

## Timer

A timer that counts down by one every cycle, and sets its flag when it reaches 0.

| Address | Register | Read | Write |
| --- | --- | --- | --- |
| `0xFFFF8200` | COUNT | the cycles left, 0 when the timer is stopped | starts counting down from this value. 0 stops the timer. |
| `0xFFFF8204` | CONTROL | the bits below | the bits below |
| `0xFFFF8208` | STATUS | bit 0: the timer expired | any value: clears bit 0 |

| CONTROL bit | Meaning |
| --- | --- |
| 0 | periodic: when the count reaches 0, start again from the last value written to COUNT |
| 1 | interrupt 1 is enabled |

- A write of N to COUNT makes the timer expire N cycles later: bit 0 of STATUS becomes 1 when the count goes from 1 to 0.
- Without the periodic bit, the timer stops at 0. With it, the count starts again right away, so the timer expires exactly every N cycles. The time a handler takes doesn't add up over the periods.
- A write to COUNT while the timer runs starts it again with the new value. A write to CONTROL doesn't change the count.
- The longest time is 2^32 - 1 cycles, almost 43 seconds.
- The flag stays 1 until STATUS is written. If the timer expires again in the same cycle as that write, the flag stays 1.

With bit 1 of CONTROL set, the flag gives interrupt 1. The handler writes STATUS to clear it, as on the [I/O devices](_index.md#interrupts) page.

A tick every millisecond (100,000 cycles):

```text
    load 3 r1
    write 0xFFFF8204 r0 r1      ; CONTROL: periodic, with the interrupt
    load32 100000 r1
    write 0xFFFF8200 r0 r1      ; COUNT: start
```

And in the handler:

```text
    write 0xFFFF8208 r0 r0      ; STATUS: clear the flag
```

Waiting for a time without an interrupt: write COUNT, then read STATUS until bit 0 is 1.
