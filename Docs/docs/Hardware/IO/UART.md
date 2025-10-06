# UART

The UART is divided into a transmitter and a receiver module, both connected to the Memory Unit.

## UART Transmitter

- **Module**: `UARTtx`
- **Baud rate**: 1 Mbaud (50 clock cycles per bit at 50MHz)
- **Data format**: 8-bit, 1 start bit, 1 stop bit, no parity
- **Operation**: Multi-cycle write operation that waits for transmission completion

## UART Receiver

- **Module**: `UARTrx`
- **Operation**: Single-cycle read operation returning the most recently received byte
- **Interrupt**: Generates `uart_irq` when new data is received
