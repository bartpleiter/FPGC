# Timers

The FPGC currently has three One Shot Timers connected to the Memory Unit:

## OS Timer

The OS Timer is a simple countdown timer that can be used for generating delays or timeouts.

- **Resolution**: Millisecond precision (configurable delay parameter)
- **Range**: 32-bit timer values
- **Operation**: Set timer value, then trigger to start countdown
- **Interrupt**: Generates interrupt when timer expires
