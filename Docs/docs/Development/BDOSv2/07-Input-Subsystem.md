# BDOS V2 Input Subsystem

**Prepared by: Sarah Chen (Embedded Systems Specialist)**  
**Contributors: James O'Brien, Elena Vasquez**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Added input event queue for games, raw Ethernet NetHID*

---

## 1. Overview

This document describes the input subsystem for BDOS V2, covering USB keyboard input, network-based HID (remote keyboard), and the input abstraction layer.

---

## 2. Hardware Input Sources

### 2.1 USB Keyboard

The FPGC has a USB keyboard interface via a CH375 USB host controller:

- **Memory-mapped I/O** at dedicated addresses
- **Scancode-based input**: Returns PS/2-style scancodes
- **No interrupt**: Requires polling or timer-based checking

### 2.2 Network HID (NetHID)

From stakeholder requirements:
> "I want remote keyboard input via Network"

This allows keyboard input to be sent over the network, useful for:
- Headless operation
- Remote development
- Testing without physical keyboard

---

## 3. HID Input Architecture

### 3.1 Unified Input Model

```
┌──────────────┐    ┌──────────────┐
│ USB Keyboard │    │  Network HID │
└──────┬───────┘    └──────┬───────┘
       │                   │
       ▼                   ▼
┌──────────────────────────────────┐
│         HID Driver Layer         │
│   (scancode → char conversion)   │
└──────────────┬───────────────────┘
               │
               ▼
┌──────────────────────────────────┐
│          HID FIFO Buffer         │
│        (Character queue)         │
└──────────────┬───────────────────┘
               │
               ▼
┌──────────────────────────────────┐
│        Input Abstraction         │
│  (stdin stream, getchar, etc.)   │
└──────────────────────────────────┘
```

### 3.2 HID FIFO Buffer

The existing implementation uses a FIFO buffer:

```c
// kernel/io/hid.h

#ifndef HID_H
#define HID_H

#define HID_FIFO_SIZE 256

struct hid_state {
    char fifo[HID_FIFO_SIZE];
    unsigned int head;
    unsigned int tail;
    unsigned int shift_held;
    unsigned int ctrl_held;
    unsigned int alt_held;
};

// Global HID state
extern struct hid_state g_hid;

// API
void hid_init(void);
void hid_poll(void);
int hid_available(void);
char hid_getchar(void);
char hid_peek(void);
void hid_push(char c);
void hid_clear(void);

// Modifier state
int hid_shift_pressed(void);
int hid_ctrl_pressed(void);
int hid_alt_pressed(void);

#endif // HID_H
```

### 3.3 FIFO Implementation

```c
// kernel/io/hid.c

#include "hid.h"

struct hid_state g_hid;

void hid_init(void) {
    g_hid.head = 0;
    g_hid.tail = 0;
    g_hid.shift_held = 0;
    g_hid.ctrl_held = 0;
    g_hid.alt_held = 0;
}

// Add character to FIFO
void hid_push(char c) {
    unsigned int next_head = (g_hid.head + 1) % HID_FIFO_SIZE;
    
    // Check for overflow
    if (next_head != g_hid.tail) {
        g_hid.fifo[g_hid.head] = c;
        g_hid.head = next_head;
    }
    // If overflow, character is dropped
}

// Check if characters available
int hid_available(void) {
    return g_hid.head != g_hid.tail;
}

// Get character from FIFO (blocking)
char hid_getchar(void) {
    // Wait for input
    while (!hid_available()) {
        hid_poll();  // Poll for new input
    }
    
    char c = g_hid.fifo[g_hid.tail];
    g_hid.tail = (g_hid.tail + 1) % HID_FIFO_SIZE;
    return c;
}

// Peek at next character without removing
char hid_peek(void) {
    if (!hid_available()) return 0;
    return g_hid.fifo[g_hid.tail];
}

// Clear all buffered input
void hid_clear(void) {
    g_hid.head = 0;
    g_hid.tail = 0;
}
```

---

## 4. USB Keyboard Driver

### 4.1 Scancode Processing

```c
// kernel/io/usb_keyboard.c

#include "hid.h"

// Hardware registers (from old_BDOS)
#define USB_DATA    (*(volatile unsigned int*)0x7C00000)
#define USB_STATUS  (*(volatile unsigned int*)0x7C00001)

// Scancode to ASCII table (US keyboard layout)
static const char scancode_to_ascii[128] = {
    0,  0,  '1', '2', '3', '4', '5', '6',     // 00-07
    '7', '8', '9', '0', '-', '=', '\b', '\t', // 08-0F
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',   // 10-17
    'o', 'p', '[', ']', '\n', 0, 'a', 's',    // 18-1F
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   // 20-27
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',   // 28-2F
    'b', 'n', 'm', ',', '.', '/', 0, '*',     // 30-37
    0, ' ', 0, 0, 0, 0, 0, 0,                  // 38-3F
    // ... function keys etc
};

// Shifted versions
static const char scancode_to_ascii_shift[128] = {
    0,  0,  '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0, 0, 0, 0, 0, 0,
    // ...
};

// Special scancodes
#define SC_SHIFT_L      0x2A
#define SC_SHIFT_R      0x36
#define SC_CTRL         0x1D
#define SC_ALT          0x38
#define SC_ESCAPE       0x01
#define SC_TAB          0x0F
#define SC_CAPS_LOCK    0x3A

// Extended scancodes (prefixed by 0xE0)
#define SC_ARROW_UP     0x48
#define SC_ARROW_DOWN   0x50
#define SC_ARROW_LEFT   0x4B
#define SC_ARROW_RIGHT  0x4D

void usb_keyboard_poll(void) {
    // Check if data available
    if (!(USB_STATUS & 0x01)) {
        return;
    }
    
    unsigned int scancode = USB_DATA;
    
    // Check for key release (bit 7 set)
    int is_release = (scancode & 0x80) != 0;
    scancode &= 0x7F;
    
    // Handle modifier keys
    if (scancode == SC_SHIFT_L || scancode == SC_SHIFT_R) {
        g_hid.shift_held = !is_release;
        return;
    }
    if (scancode == SC_CTRL) {
        g_hid.ctrl_held = !is_release;
        return;
    }
    if (scancode == SC_ALT) {
        g_hid.alt_held = !is_release;
        return;
    }
    
    // Only process key press, not release
    if (is_release) return;
    
    // Handle special keys
    if (scancode == SC_ESCAPE) {
        hid_push(0x1B);  // ESC character
        return;
    }
    
    // Handle arrow keys (generate ANSI escape sequences)
    if (scancode == SC_ARROW_UP) {
        hid_push(0x1B); hid_push('['); hid_push('A');
        return;
    }
    if (scancode == SC_ARROW_DOWN) {
        hid_push(0x1B); hid_push('['); hid_push('B');
        return;
    }
    if (scancode == SC_ARROW_RIGHT) {
        hid_push(0x1B); hid_push('['); hid_push('C');
        return;
    }
    if (scancode == SC_ARROW_LEFT) {
        hid_push(0x1B); hid_push('['); hid_push('D');
        return;
    }
    
    // Convert to ASCII
    char c;
    if (g_hid.shift_held) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }
    
    // Handle Ctrl combinations
    if (g_hid.ctrl_held && c >= 'a' && c <= 'z') {
        c = c - 'a' + 1;  // Ctrl+A = 0x01, etc.
    }
    
    if (c != 0) {
        hid_push(c);
    }
}
```

---

## 5. Network HID Driver

### 5.1 NetHID Protocol (Raw Ethernet)

NetHID uses the BDOS raw Ethernet protocol (see Document 08):

```
NetHID Packet (inside BDOS frame):
- BDOS header type = BDOS_PKT_HID (0x30)
- Payload contains HID event data

HID Event Payload:
+------------+----------+----------+----------+----------+
| Event Type | Reserved | Keycode  | Mouse DX | Mouse DY |
| (1 byte)   | (1 byte) | (2 bytes)| (2 bytes)| (2 bytes)|
+------------+----------+----------+----------+----------+
| Buttons    | Modifiers|
| (1 byte)   | (1 byte) |
+------------+----------+
```

### 5.2 NetHID Implementation

```c
// kernel/io/nethid.c

#include "hid.h"
#include "input_queue.h"
#include "kernel/net/bdos_proto.h"

// HID event types (in BDOS packet)
#define HID_KEY_DOWN    0x01
#define HID_KEY_UP      0x02
#define HID_MOUSE_MOVE  0x10
#define HID_MOUSE_BTN   0x11

void nethid_process_packet(unsigned char* payload, unsigned int len) {
    if (len < 2) return;
    
    unsigned char event_type = payload[0];
    unsigned char modifiers = payload[9];
    
    // Update modifier state
    g_hid.shift_held = (modifiers & 0x01) != 0;
    g_hid.ctrl_held  = (modifiers & 0x02) != 0;
    g_hid.alt_held   = (modifiers & 0x04) != 0;
    
    // Create input event
    struct input_event event;
    event.timestamp = get_system_time_low();
    
    switch (event_type) {
        case HID_KEY_DOWN:
        case HID_KEY_UP:
            event.type = INPUT_EVENT_KEY;
            event.key.scancode = payload[2] | (payload[3] << 8);
            event.key.pressed = (event_type == HID_KEY_DOWN);
            event.key.modifiers = modifiers;
            event.key.ascii = scancode_to_ascii(event.key.scancode, g_hid.shift_held);
            
            // Push to event queue
            input_queue_push(&g_events, &event);
            
            // Also push ASCII to character FIFO for shell
            if (event.key.pressed && event.key.ascii != 0) {
                hid_push(event.key.ascii);
            }
            break;
            
        case HID_MOUSE_MOVE:
            event.type = INPUT_EVENT_MOUSE_MOVE;
            event.mouse.dx = (signed short)(payload[4] | (payload[5] << 8));
            event.mouse.dy = (signed short)(payload[6] | (payload[7] << 8));
            event.mouse.buttons = payload[8];
            input_queue_push(&g_events, &event);
            break;
            
        case HID_MOUSE_BTN:
            event.type = INPUT_EVENT_MOUSE_BTN;
            event.mouse.dx = 0;
            event.mouse.dy = 0;
            event.mouse.buttons = payload[8];
            input_queue_push(&g_events, &event);
            break;
    }
}
```

### 5.3 NetHID Integration with Network Subsystem

NetHID packets are handled by the network dispatcher when a BDOS frame with type `BDOS_PKT_HID` arrives:

```c
// kernel/net/bdos_dispatch.c

void bdos_dispatch_packet(struct bdos_header* hdr, unsigned char* payload,
                          unsigned int len, unsigned char* src_mac) {
    switch (hdr->type) {
        case BDOS_PKT_HID:
            nethid_process_packet(payload, len);
            break;
            
        case BDOS_PKT_DISCOVER:
            net_handle_discover(src_mac);
            break;
            
        // ... other packet types ...
    }
}
```

---

## 6. Combined HID Poll

### 6.1 Unified Polling

```c
// kernel/io/hid.c

void hid_poll(void) {
    // Poll USB keyboard
    usb_keyboard_poll();
    
    // Poll network HID (if network available)
    nethid_poll();
}
```

### 6.2 Integration with Timer

To avoid missing keystrokes, HID polling should happen regularly:

```c
// kernel/timer/timer.c

#include "hid.h"

void timer_interrupt_handler(void) {
    // Update system time
    g_system_ticks++;
    
    // Poll HID periodically (every 10ms)
    if ((g_system_ticks % 10) == 0) {
        hid_poll();
    }
    
    // Scheduler tick (if timer-based preemption)
    sched_tick();
}
```

---

## 7. Input Abstraction Layer

### 7.1 stdin Stream

The input abstraction provides `stdin` as a character stream:

```c
// kernel/io/stdin.c

#include "hid.h"
#include "stream.h"

// stdin read function
static int stdin_read(struct stream* s, void* buf, unsigned int count) {
    char* cbuf = (char*)buf;
    unsigned int read = 0;
    
    while (read < count) {
        // Check for available input
        if (!hid_available()) {
            // Non-blocking: return what we have
            if (read > 0) break;
            
            // Blocking: wait for at least one character
            while (!hid_available()) {
                hid_poll();
                // Could yield to scheduler here
            }
        }
        
        cbuf[read++] = hid_getchar();
    }
    
    return read;
}

// Create stdin stream
struct stream stdin_stream = {
    .read = stdin_read,
    .write = NULL,  // stdin is read-only
    .close = NULL,
    .user_data = NULL,
};

struct stream* get_stdin(void) {
    return &stdin_stream;
}
```

### 7.2 Line-Buffered Input

For shell-style input, provide line reading:

```c
// kernel/io/readline.c

#include "hid.h"
#include "term.h"

#define MAX_LINE_LENGTH 256

// Read a line with editing support
int readline(char* buf, unsigned int size, const char* prompt) {
    if (prompt) term_puts(prompt);
    
    unsigned int pos = 0;
    unsigned int len = 0;
    
    while (1) {
        char c = hid_getchar();
        
        switch (c) {
            case '\n':
            case '\r':
                // End of line
                buf[len] = '\0';
                term_putchar('\n');
                return len;
                
            case '\b':
            case 0x7F:  // DEL
                // Backspace
                if (pos > 0) {
                    // Move characters left
                    for (unsigned int i = pos - 1; i < len - 1; i++) {
                        buf[i] = buf[i + 1];
                    }
                    pos--;
                    len--;
                    
                    // Update display
                    term_putchar('\b');
                    term_puts(&buf[pos]);
                    term_putchar(' ');
                    for (unsigned int i = pos; i <= len; i++) {
                        term_putchar('\b');
                    }
                }
                break;
                
            case 0x03:  // Ctrl+C
                buf[0] = '\0';
                term_puts("^C\n");
                return -1;  // Interrupted
                
            case 0x1B:  // Escape (arrow keys)
                // Check for arrow key sequence
                if (hid_peek() == '[') {
                    hid_getchar();  // Consume '['
                    char arrow = hid_getchar();
                    
                    switch (arrow) {
                        case 'C':  // Right
                            if (pos < len) {
                                term_putchar(buf[pos]);
                                pos++;
                            }
                            break;
                        case 'D':  // Left
                            if (pos > 0) {
                                term_putchar('\b');
                                pos--;
                            }
                            break;
                        // Up/Down for history (TODO)
                    }
                }
                break;
                
            default:
                // Regular character
                if (len < size - 1 && c >= 0x20 && c < 0x7F) {
                    // Insert character
                    for (unsigned int i = len; i > pos; i--) {
                        buf[i] = buf[i - 1];
                    }
                    buf[pos] = c;
                    len++;
                    
                    // Update display
                    term_puts(&buf[pos]);
                    pos++;
                    for (unsigned int i = pos; i < len; i++) {
                        term_putchar('\b');
                    }
                }
                break;
        }
    }
}
```

---

## 8. Alt-Tab Process Switching

### 8.1 Handling Alt-Tab

From stakeholder requirements:
> "Ability to switch between running processes via alt-tab"

```c
// kernel/io/hid.c

void hid_poll(void) {
    usb_keyboard_poll();
    nethid_poll();
    
    // Check for Alt-Tab
    if (g_hid.alt_held && hid_peek() == '\t') {
        hid_getchar();  // Consume the tab
        
        // Trigger process switch
        proc_switch_next();
        
        // Visual feedback
        term_puts("\n[Switched to: ");
        term_puts(proc_get_current()->name);
        term_puts("]\n");
    }
}
```

### 8.2 Process Switching Integration

```c
// kernel/proc/switch.c

void proc_switch_next(void) {
    struct process* current = proc_get_current();
    struct process* next = proc_ring_next(current);
    
    if (next != current) {
        // Save current state
        proc_save_state(current);
        
        // Restore next state
        proc_restore_state(next);
        
        // Switch focus
        proc_set_current(next);
    }
}
```

---

## 9. Input Routing

### 9.1 Per-Process Input

Each process has its own stdin:

```c
// kernel/proc/process.h

struct process {
    // ... other fields
    
    // I/O streams
    struct stream* stdin;
    struct stream* stdout;
    struct stream* stderr;
    
    // Input state
    int has_focus;          // Receives keyboard input?
};
```

### 9.2 Input Focus

Only the foreground process receives keyboard input:

```c
// kernel/io/input_router.c

void input_route_char(char c) {
    struct process* fg = proc_get_foreground();
    
    if (fg != NULL && fg->stdin != NULL) {
        // Route to process's stdin buffer
        stream_push(fg->stdin, c);
    }
}

// Modified HID push
void hid_push_routed(char c) {
    // Check for system hotkeys first
    if (handle_system_hotkey(c)) {
        return;
    }
    
    // Route to foreground process
    input_route_char(c);
}
```

---

## 10. System Hotkeys

### 10.1 Reserved Key Combinations

```c
// kernel/io/hotkeys.c

int handle_system_hotkey(char c) {
    // Alt+Tab: Switch process (handled in hid_poll)
    // Already handled before character is pushed
    
    // Ctrl+C: Interrupt foreground process
    if (g_hid.ctrl_held && c == 'c') {
        struct process* fg = proc_get_foreground();
        if (fg != NULL) {
            proc_send_signal(fg, SIG_INT);
        }
        return 1;  // Handled
    }
    
    // Ctrl+Z: Suspend foreground process (future)
    if (g_hid.ctrl_held && c == 'z') {
        // proc_suspend(proc_get_foreground());
        return 1;
    }
    
    // Alt+F4: Kill foreground process (future)
    if (g_hid.alt_held && c == /* F4 scancode */) {
        // proc_kill(proc_get_foreground());
        return 1;
    }
    
    return 0;  // Not a system hotkey
}
```

---

## 11. Input API for User Programs

### 11.1 User Library Functions

```c
// libs/user/io/input.h

// Basic character input
int getchar(void);              // Blocking read, returns char or EOF
int getchar_nonblock(void);     // Non-blocking, returns -1 if none

// Line input
char* gets(char* buf);          // DEPRECATED, unsafe
int fgets(char* buf, int size, int fd);

// Check input availability
int kbhit(void);                // Returns 1 if input available

// Raw input (bypass line editing)
int getch(void);                // Raw character, no echo
int getche(void);               // Raw character, with echo

// Modifier state
int shift_pressed(void);
int ctrl_pressed(void);
int alt_pressed(void);
```

### 11.2 Syscall Implementations

```c
// libs/user/io/input.c

#include "syscall.h"

int getchar(void) {
    int result;
    
    // Syscall: SYS_READ with stdin (fd 0)
    asm(
        "load32 %[syscall_nr], r1\n"
        "load32 %[fd], r2\n"
        "load32 1, r3\n"          // count = 1
        "load32 %[result_addr], r4\n"  // buffer
        "savpc r15\n"
        "jump @SYS_ENTRY\n"
        "read32 0, r15, %[result]\n"
        : [result] "=r" (result)
        : [syscall_nr] "i" (SYS_READ),
          [fd] "i" (0),
          [result_addr] "r" (&result)
        : "r1", "r2", "r3", "r4", "r15"
    );
    
    return result;
}

int kbhit(void) {
    int result;
    
    // Syscall: SYS_KBHIT
    asm(
        "load32 %[syscall_nr], r1\n"
        "savpc r15\n"
        "jump @SYS_ENTRY\n"
        "read32 0, r15, %[result]\n"
        : [result] "=r" (result)
        : [syscall_nr] "i" (SYS_KBHIT)
        : "r1", "r15"
    );
    
    return result;
}
```

---

## 12. Input Event Queue (For Games/Doom)

### 12.1 Rationale

For games like Doom, a character-based FIFO is insufficient:
- Games need to detect key press AND release events
- Multiple simultaneous keys must be tracked
- Low latency is critical

### 12.2 Event Structure

```c
// kernel/io/input_event.h

#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H

// Event types
#define INPUT_EVENT_KEY         0x01
#define INPUT_EVENT_MOUSE_MOVE  0x02
#define INPUT_EVENT_MOUSE_BTN   0x03

// Key event structure
struct key_event {
    unsigned short scancode;    // Raw scancode
    unsigned char pressed;      // 1 = pressed, 0 = released
    unsigned char modifiers;    // Shift/Ctrl/Alt state
    char ascii;                 // ASCII character (0 if none)
    unsigned char reserved[3];
};

// Mouse event structure
struct mouse_event {
    signed short dx;            // X movement
    signed short dy;            // Y movement
    unsigned char buttons;      // Button state (bit 0=left, 1=right, 2=middle)
    unsigned char reserved[3];
};

// Generic input event
struct input_event {
    unsigned char type;         // INPUT_EVENT_*
    unsigned char reserved;
    unsigned short timestamp;   // Low 16 bits of system time (wraps)
    union {
        struct key_event key;
        struct mouse_event mouse;
    };
};

#define INPUT_EVENT_SIZE    12  // sizeof(struct input_event)

#endif // INPUT_EVENT_H
```

### 12.3 Event Queue

```c
// kernel/io/input_queue.h

#define INPUT_QUEUE_SIZE    64

struct input_queue {
    struct input_event events[INPUT_QUEUE_SIZE];
    unsigned int head;
    unsigned int tail;
};

// API
void input_queue_init(struct input_queue* q);
int input_queue_push(struct input_queue* q, struct input_event* event);
int input_queue_pop(struct input_queue* q, struct input_event* event);
int input_queue_peek(struct input_queue* q, struct input_event* event);
int input_queue_count(struct input_queue* q);
```

### 12.4 Implementation

```c
// kernel/io/input_queue.c

#include "input_queue.h"

void input_queue_init(struct input_queue* q) {
    q->head = 0;
    q->tail = 0;
}

int input_queue_push(struct input_queue* q, struct input_event* event) {
    unsigned int next_head = (q->head + 1) % INPUT_QUEUE_SIZE;
    
    if (next_head == q->tail) {
        return -1;  // Queue full
    }
    
    q->events[q->head] = *event;
    q->head = next_head;
    return 0;
}

int input_queue_pop(struct input_queue* q, struct input_event* event) {
    if (q->head == q->tail) {
        return -1;  // Queue empty
    }
    
    *event = q->events[q->tail];
    q->tail = (q->tail + 1) % INPUT_QUEUE_SIZE;
    return 0;
}

int input_queue_count(struct input_queue* q) {
    if (q->head >= q->tail) {
        return q->head - q->tail;
    }
    return INPUT_QUEUE_SIZE - q->tail + q->head;
}
```

### 12.5 Dual Mode: Character FIFO + Event Queue

The system maintains both:
- **Character FIFO**: For shell, text apps (existing)
- **Event Queue**: For games needing low-level input

```c
// kernel/io/hid.c

// Global queues
struct hid_state g_hid;         // Character FIFO
struct input_queue g_events;     // Event queue

void usb_keyboard_process(unsigned int scancode) {
    int is_release = (scancode & 0x80) != 0;
    scancode &= 0x7F;
    
    // Create event for event queue
    struct input_event event;
    event.type = INPUT_EVENT_KEY;
    event.timestamp = get_system_time_low();
    event.key.scancode = scancode;
    event.key.pressed = !is_release;
    event.key.modifiers = (g_hid.shift_held ? 0x01 : 0) |
                          (g_hid.ctrl_held ? 0x02 : 0) |
                          (g_hid.alt_held ? 0x04 : 0);
    event.key.ascii = scancode_to_ascii(scancode, g_hid.shift_held);
    
    // Push to event queue (for games)
    input_queue_push(&g_events, &event);
    
    // Also push to character FIFO if it's a key press with ASCII
    if (!is_release && event.key.ascii != 0) {
        hid_push(event.key.ascii);
    }
}
```

### 12.6 Syscall Interface

```c
// Syscall: Poll for input event
int sys_input_poll(struct input_event* event) {
    if (validate_user_buffer(event, sizeof(struct input_event)) < 0) {
        return -EFAULT;
    }
    
    return input_queue_pop(&g_events, event);
}

// Syscall: Peek for input event
int sys_input_peek(void) {
    return input_queue_count(&g_events);
}
```

### 12.7 Usage Example (Doom-style)

```c
// Game main loop
void game_main(void) {
    struct input_event event;
    int keys_held[256] = {0};  // Track held keys
    
    while (running) {
        // Process all pending events
        while (input_poll(&event) == 0) {
            if (event.type == INPUT_EVENT_KEY) {
                keys_held[event.key.scancode] = event.key.pressed;
                
                // Handle specific keys
                if (event.key.scancode == SC_ESCAPE && event.key.pressed) {
                    open_menu();
                }
            }
        }
        
        // Use held key state for movement
        if (keys_held[SC_W]) move_forward();
        if (keys_held[SC_S]) move_backward();
        if (keys_held[SC_A]) strafe_left();
        if (keys_held[SC_D]) strafe_right();
        
        // Render frame
        render();
    }
}
```

---

## 13. Design Alternatives

### Alternative A: Interrupt-Driven Input

**Concept**: Use interrupts for keyboard input instead of polling.

**Pros:**
- Lower latency
- CPU can sleep between inputs
- No missed keystrokes

**Cons:**
- Hardware may not support keyboard interrupts
- More complex interrupt handling
- Need to be careful about race conditions

**Verdict**: If hardware supports it, worth implementing. Start with polling.

### Alternative B: Input Event Queue

**Status: ✅ IMPLEMENTED** (see Section 12)

This is now part of the design for supporting games like Doom.

### Alternative C: Separate Input Process

**Concept**: Input handling as a separate process, communicating via IPC.

**Pros:**
- Clean separation
- Can restart input handler independently

**Cons:**
- IPC overhead
- More complex

**Verdict**: Overkill for this project.

---

## 13. Implementation Checklist

- [ ] Implement HID FIFO buffer
- [ ] Implement USB keyboard driver with scancode conversion
- [ ] Create scancode-to-ASCII tables (US layout)
- [ ] Handle modifier keys (Shift, Ctrl, Alt)
- [ ] Handle special keys (arrows, function keys)
- [ ] Implement NetHID protocol
- [ ] Implement NetHID packet processing
- [ ] Create unified `hid_poll()` function
- [ ] Implement stdin stream
- [ ] Implement `readline()` with line editing
- [ ] Implement Alt-Tab process switching
- [ ] Implement system hotkeys (Ctrl+C)
- [ ] Create user library input functions
- [ ] Test with USB keyboard
- [ ] Test with network HID client

---

## 15. Summary

| Component | Implementation | Notes |
|-----------|----------------|-------|
| USB Keyboard | Polling driver | Scancode conversion |
| Network HID | Raw Ethernet (BDOS protocol) | No UDP, direct MAC |
| Character Buffer | 256-char FIFO | For shell/text apps |
| Event Queue | 64-event queue | For games (key up/down) |
| stdin | Stream interface | Per-process |
| Line Editing | readline() | Backspace, arrows |
| Alt-Tab | Alt+Tab hotkey | Process ring switch |
| User API | getchar/kbhit | Via syscalls |
