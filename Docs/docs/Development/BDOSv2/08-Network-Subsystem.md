# BDOS V2 Network Subsystem

**Prepared by: Sarah Chen (Embedded Systems Specialist)**  
**Contributors: Elena Vasquez, Marcus Rodriguez**  
**Date: December 2024**  
**Version: 1.1**

*Revision 1.1: Complete rewrite for raw Ethernet - no IP/UDP/ARP stack, custom simple protocol*

---

## 1. Overview

This document describes the network subsystem for BDOS V2. Following the BRFS design philosophy of "simple and custom", the network layer operates at the **raw Ethernet frame level** without IP, UDP, ARP, or other standard protocols.

### 1.1 Design Philosophy

From stakeholder direction:
> "I don't want to build a whole IP/ARP/UDP/ICMP protocol... Just raw Ethernet with MAC addresses. Simple custom protocol for sending programs and HID events."

Key principles:
- **Raw Ethernet frames**: Direct MAC-to-MAC communication
- **Custom protocol**: Simple, purpose-built packet format
- **No IP stack**: No ARP tables, no routing, no DNS
- **Controlled network**: All devices on same LAN segment

---

## 2. Hardware Platform

### 2.1 ENC28J60 Controller

The ENC28J60 is a standalone 10BASE-T Ethernet controller:

| Feature | Value |
|---------|-------|
| Interface | SPI (4-wire) |
| Speed | 10 Mbps |
| Buffer | 8 KB shared TX/RX |
| MAC | Built-in with auto-CRC |
| PHY | Integrated 10BASE-T |

### 2.2 Memory-Mapped I/O

```c
// kernel/net/enc28j60_hw.h

#define ENC_SPI_DATA    (*(volatile unsigned int*)0x7C00010)
#define ENC_SPI_CS      (*(volatile unsigned int*)0x7C00011)
#define ENC_INT_STATUS  (*(volatile unsigned int*)0x7C00012)
```

---

## 3. Raw Ethernet Frame Format

### 3.1 Standard Ethernet Frame

```
┌──────────────┬──────────────┬───────────┬─────────────────┬─────┐
│ Dest MAC     │ Source MAC   │ EtherType │ Payload         │ FCS │
│ (6 bytes)    │ (6 bytes)    │ (2 bytes) │ (46-1500 bytes) │(4b) │
└──────────────┴──────────────┴───────────┴─────────────────┴─────┘
```

### 3.2 BDOS Custom EtherType

We use a custom EtherType to identify BDOS packets:

```c
#define ETH_TYPE_BDOS   0xBD05  // Custom BDOS protocol
```

**Note**: EtherTypes 0x0000-0x05DC are reserved for length field (802.3). Custom types should be ≥0x0600. 0xBD05 is in the safe range.

---

## 4. BDOS Protocol

### 4.1 Packet Header

All BDOS packets share a common header after the Ethernet header:

```c
// include/net/bdos_proto.h

#define BDOS_PROTO_VERSION  1

// Packet types
#define BDOS_PKT_DISCOVER   0x01    // Device discovery
#define BDOS_PKT_ANNOUNCE   0x02    // Device announcement
#define BDOS_PKT_DATA       0x10    // Generic data transfer
#define BDOS_PKT_DATA_ACK   0x11    // Data acknowledgment
#define BDOS_PKT_PROGRAM    0x20    // Program upload start
#define BDOS_PKT_PROG_DATA  0x21    // Program data chunk
#define BDOS_PKT_PROG_END   0x22    // Program upload complete
#define BDOS_PKT_PROG_ACK   0x23    // Program chunk acknowledgment
#define BDOS_PKT_HID        0x30    // HID event (keyboard/mouse)
#define BDOS_PKT_EXEC       0x40    // Execute command
#define BDOS_PKT_RESULT     0x41    // Command result

// BDOS packet header (immediately after Ethernet header)
struct bdos_header {
    unsigned char version;      // Protocol version (1)
    unsigned char type;         // Packet type
    unsigned short seq;         // Sequence number
    unsigned short length;      // Payload length (bytes after header)
    unsigned short checksum;    // Simple checksum of payload
};

#define BDOS_HEADER_SIZE    8
```

### 4.2 Complete Frame Layout

```
┌──────────────────────────────────────────────────────────────────┐
│                    ETHERNET HEADER (14 bytes)                    │
├──────────────┬──────────────┬───────────────────────────────────┤
│ Dest MAC     │ Source MAC   │ EtherType = 0xBD05                │
│ (6 bytes)    │ (6 bytes)    │ (2 bytes)                         │
├──────────────┴──────────────┴───────────────────────────────────┤
│                    BDOS HEADER (8 bytes)                         │
├─────────┬─────────┬──────────┬──────────┬───────────────────────┤
│ Version │ Type    │ Sequence │ Length   │ Checksum              │
│ (1 byte)│ (1 byte)│ (2 bytes)│ (2 bytes)│ (2 bytes)             │
├─────────┴─────────┴──────────┴──────────┴───────────────────────┤
│                    PAYLOAD (0-1478 bytes)                        │
│                    (type-specific data)                          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. NetLoader Protocol

### 5.1 Purpose

NetLoader allows uploading programs from a PC to BDOS over the network.

### 5.2 Upload Sequence

```
PC                                      FPGC
│                                       │
│ ──── DISCOVER (broadcast) ──────────► │
│                                       │
│ ◄──── ANNOUNCE (unicast) ──────────── │
│       (sends MAC, device name)        │
│                                       │
│ ──── PROGRAM (start upload) ────────► │
│       (filename, total size)          │
│                                       │
│ ──── PROG_DATA (chunk 0) ───────────► │
│                                       │
│ ◄──── PROG_ACK (ack chunk 0) ──────── │
│                                       │
│ ──── PROG_DATA (chunk 1) ───────────► │
│                                       │
│ ◄──── PROG_ACK (ack chunk 1) ──────── │
│                                       │
│      ... repeat for all chunks ...    │
│                                       │
│ ──── PROG_END ──────────────────────► │
│       (final checksum)                │
│                                       │
│ ◄──── PROG_ACK (success/fail) ─────── │
│                                       │
```

### 5.3 Packet Structures

```c
// Program upload start
struct prog_start_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_PROGRAM
    unsigned int total_size;    // Total program size in bytes
    char filename[32];          // Destination filename
};

// Program data chunk
struct prog_data_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_PROG_DATA
    unsigned int offset;        // Byte offset in file
    unsigned int chunk_size;    // Size of this chunk (max 1024)
    unsigned char data[1024];   // Chunk data
};

// Program acknowledgment
struct prog_ack_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_PROG_ACK
    unsigned int acked_offset;  // Acknowledged up to this offset
    unsigned char status;       // 0 = OK, 1 = checksum error, 2 = disk full
};

// Program end
struct prog_end_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_PROG_END
    unsigned int final_size;    // Should match total_size
    unsigned int checksum32;    // CRC32 of entire file
};
```

### 5.4 Chunk Size Selection

| Chunk Size | Packets for 2MB | Pros | Cons |
|------------|-----------------|------|------|
| 256 bytes | 8192 | Fits small buffer | High overhead |
| 512 bytes | 4096 | Good balance | - |
| 1024 bytes | 2048 | ✅ Recommended | Needs 1KB buffer |

**Recommended: 1024 bytes per chunk**
- 2 MB program = 2048 chunks
- At 10 Mbps = ~2 seconds theoretical
- With ACK overhead = ~5-10 seconds realistic

---

## 6. NetHID Protocol

### 6.1 Purpose

NetHID allows a PC to send keyboard and mouse events to BDOS, useful for:
- Debugging without physical keyboard
- Remote control
- Using PC keyboard when FPGC keyboard is unavailable

### 6.2 HID Event Packet

```c
// HID event types
#define HID_KEY_DOWN    0x01
#define HID_KEY_UP      0x02
#define HID_MOUSE_MOVE  0x10
#define HID_MOUSE_BTN   0x11

// HID event packet
struct hid_event_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_HID
    unsigned char event_type;   // HID_KEY_DOWN, etc.
    unsigned char reserved;
    unsigned short keycode;     // Key scancode (for keyboard)
    signed short mouse_dx;      // Mouse delta X (for mouse)
    signed short mouse_dy;      // Mouse delta Y
    unsigned char buttons;      // Mouse button state
    unsigned char modifiers;    // Shift/Ctrl/Alt state
};
```

### 6.3 Integration with Input Subsystem

NetHID events are injected into the same input queue as local keyboard events:

```c
// kernel/net/nethid.c

void nethid_process_packet(struct hid_event_packet* pkt) {
    struct input_event event;
    
    if (pkt->event_type == HID_KEY_DOWN || pkt->event_type == HID_KEY_UP) {
        event.type = INPUT_EVENT_KEY;
        event.key.scancode = pkt->keycode;
        event.key.pressed = (pkt->event_type == HID_KEY_DOWN);
        event.key.modifiers = pkt->modifiers;
    } else if (pkt->event_type == HID_MOUSE_MOVE) {
        event.type = INPUT_EVENT_MOUSE_MOVE;
        event.mouse.dx = pkt->mouse_dx;
        event.mouse.dy = pkt->mouse_dy;
    } else if (pkt->event_type == HID_MOUSE_BTN) {
        event.type = INPUT_EVENT_MOUSE_BTN;
        event.mouse.buttons = pkt->buttons;
    }
    
    // Add to input queue (same as local input)
    input_queue_push(&event);
}
```

---

## 7. Device Discovery

### 7.1 Discovery Protocol

A simple discovery mechanism for finding BDOS devices on the network:

```c
// Discovery request (broadcast)
struct discover_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_DISCOVER
    // No payload - just asking "who's there?"
};

// Announcement response (unicast to requester)
struct announce_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_ANNOUNCE
    unsigned char mac[6];       // This device's MAC
    unsigned short reserved;
    char device_name[16];       // e.g., "FPGC-01"
    char os_version[8];         // e.g., "BDOS2.0"
    unsigned int capabilities;  // Bitmask of supported features
};

// Capability flags
#define CAP_NETLOADER   0x0001  // Supports program upload
#define CAP_NETHID      0x0002  // Supports remote HID
#define CAP_FILESYSTEM  0x0004  // Supports file transfer
#define CAP_EXEC        0x0008  // Supports remote exec
```

### 7.2 Discovery Flow

```c
// kernel/net/discover.c

void net_handle_discover(unsigned char* src_mac) {
    struct announce_packet pkt;
    
    // Fill header
    pkt.hdr.version = BDOS_PROTO_VERSION;
    pkt.hdr.type = BDOS_PKT_ANNOUNCE;
    pkt.hdr.seq = 0;
    pkt.hdr.length = sizeof(pkt) - sizeof(struct bdos_header);
    
    // Fill payload
    net_get_mac(pkt.mac);
    strcpy(pkt.device_name, config_get_device_name());
    strcpy(pkt.os_version, "BDOS2.0");
    pkt.capabilities = CAP_NETLOADER | CAP_NETHID | CAP_FILESYSTEM;
    
    // Calculate checksum
    pkt.hdr.checksum = calc_checksum(&pkt.mac, pkt.hdr.length);
    
    // Send unicast response to requester
    net_send_raw(src_mac, ETH_TYPE_BDOS, (unsigned char*)&pkt, sizeof(pkt));
}
```

---

## 8. Cluster Communication

### 8.1 Purpose

Multiple FPGC devices can communicate for distributed computing:
- Task distribution
- Result aggregation
- Simple message passing

### 8.2 Data Transfer Packets

```c
// Generic data packet for cluster communication
struct data_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_DATA
    unsigned int msg_id;        // Message identifier
    unsigned int msg_type;      // Application-defined message type
    unsigned char data[1024];   // Message payload
};

// Data acknowledgment
struct data_ack_packet {
    struct bdos_header hdr;     // type = BDOS_PKT_DATA_ACK
    unsigned int acked_id;      // Which message is acknowledged
    unsigned char status;       // 0 = received OK
};
```

### 8.3 Example: Distributed Computation

```c
// Master node distributes work
void master_distribute_work(unsigned char targets[][6], int target_count,
                           int* work_items, int work_count) {
    int items_per_target = work_count / target_count;
    int i;
    
    for (i = 0; i < target_count; i++) {
        struct data_packet pkt;
        pkt.hdr.type = BDOS_PKT_DATA;
        pkt.msg_type = MSG_WORK_ASSIGNMENT;
        
        // Pack work items for this target
        int start = i * items_per_target;
        int count = (i == target_count - 1) ? 
                    (work_count - start) : items_per_target;
        
        memcpy(pkt.data, &work_items[start], count * sizeof(int));
        pkt.hdr.length = count * sizeof(int);
        
        net_send_raw(targets[i], ETH_TYPE_BDOS, (unsigned char*)&pkt, 
                     sizeof(struct bdos_header) + pkt.hdr.length);
    }
}

// Worker node processes and returns result
void worker_process_and_respond(struct data_packet* work, unsigned char* master_mac) {
    int result = 0;
    int* items = (int*)work->data;
    int count = work->hdr.length / sizeof(int);
    int i;
    
    // Do computation
    for (i = 0; i < count; i++) {
        result += compute_something(items[i]);
    }
    
    // Send result back
    struct data_packet response;
    response.hdr.type = BDOS_PKT_DATA;
    response.msg_type = MSG_WORK_RESULT;
    memcpy(response.data, &result, sizeof(int));
    response.hdr.length = sizeof(int);
    
    net_send_raw(master_mac, ETH_TYPE_BDOS, (unsigned char*)&response,
                 sizeof(struct bdos_header) + sizeof(int));
}
```

---

## 9. ENC28J60 Driver

### 9.1 Driver Interface

```c
// kernel/net/enc28j60.h

#ifndef ENC28J60_H
#define ENC28J60_H

// Initialization
int enc_init(void);
int enc_link_up(void);

// Raw frame I/O
int enc_send_frame(unsigned char* frame, unsigned int len);
int enc_recv_frame(unsigned char* buf, unsigned int maxlen);
int enc_frames_available(void);

// MAC address
void enc_set_mac(unsigned char mac[6]);
void enc_get_mac(unsigned char mac[6]);

// Statistics
unsigned int enc_get_tx_count(void);
unsigned int enc_get_rx_count(void);

#endif
```

### 9.2 Send Implementation

```c
// kernel/net/enc28j60.c

int enc_send_frame(unsigned char* frame, unsigned int len) {
    if (len < 14 || len > 1518) {
        return -1;  // Invalid frame size
    }
    
    // Wait for previous TX to complete
    while (enc_read_reg(ECON1) & ECON1_TXRTS) {
        // Busy wait (could yield here)
    }
    
    // Set write pointer to TX buffer start
    enc_write_reg(EWRPTL, TXSTART & 0xFF);
    enc_write_reg(EWRPTH, TXSTART >> 8);
    
    // Write per-packet control byte (0x00 = use MACON3 settings)
    enc_write_buffer(0x00);
    
    // Write frame data
    enc_write_buffer_block(frame, len);
    
    // Set TX end pointer
    unsigned int txend = TXSTART + len;
    enc_write_reg(ETXNDL, txend & 0xFF);
    enc_write_reg(ETXNDH, txend >> 8);
    
    // Start transmission
    enc_bit_set(ECON1, ECON1_TXRTS);
    
    return len;
}
```

### 9.3 Receive Implementation

```c
int enc_recv_frame(unsigned char* buf, unsigned int maxlen) {
    // Check if packets available
    if (enc_read_reg(EPKTCNT) == 0) {
        return 0;  // No packets
    }
    
    // Set read pointer to next packet
    enc_write_reg(ERDPTL, next_packet_ptr & 0xFF);
    enc_write_reg(ERDPTH, next_packet_ptr >> 8);
    
    // Read next packet pointer
    unsigned int next_pkt = enc_read_buffer();
    next_pkt |= enc_read_buffer() << 8;
    
    // Read receive status vector
    unsigned int rx_len = enc_read_buffer();
    rx_len |= enc_read_buffer() << 8;
    unsigned int status = enc_read_buffer();
    status |= enc_read_buffer() << 8;
    
    // Check for valid frame
    if (!(status & 0x80)) {
        // Invalid frame, skip it
        enc_write_reg(ERXRDPTL, next_pkt & 0xFF);
        enc_write_reg(ERXRDPTH, next_pkt >> 8);
        enc_bit_set(ECON2, ECON2_PKTDEC);
        next_packet_ptr = next_pkt;
        return -1;
    }
    
    // Read frame data (subtract 4 for CRC)
    unsigned int data_len = rx_len - 4;
    if (data_len > maxlen) data_len = maxlen;
    
    enc_read_buffer_block(buf, data_len);
    
    // Update read pointer and decrement packet count
    enc_write_reg(ERXRDPTL, next_pkt & 0xFF);
    enc_write_reg(ERXRDPTH, next_pkt >> 8);
    enc_bit_set(ECON2, ECON2_PKTDEC);
    next_packet_ptr = next_pkt;
    
    return data_len;
}
```

---

## 10. Network Syscalls

### 10.1 Raw Ethernet Syscalls

```c
// Syscall implementations

// Send raw Ethernet frame
int sys_eth_send(unsigned char* frame, int len) {
    // Validate buffer
    if (validate_user_buffer(frame, len) < 0) {
        return -EFAULT;
    }
    
    if (len < 14 || len > 1518) {
        return -EINVAL;
    }
    
    return enc_send_frame(frame, len);
}

// Receive raw Ethernet frame
int sys_eth_recv(unsigned char* buf, int maxlen) {
    if (validate_user_buffer(buf, maxlen) < 0) {
        return -EFAULT;
    }
    
    return enc_recv_frame(buf, maxlen);
}

// Check if frames available
int sys_eth_available(void) {
    return enc_frames_available();
}

// Get local MAC address
void sys_eth_getmac(unsigned char* mac) {
    if (validate_user_buffer(mac, 6) < 0) {
        return;
    }
    enc_get_mac(mac);
}
```

---

## 11. PC-Side Tools

### 11.1 NetLoader Tool (Python)

```python
#!/usr/bin/env python3
"""
BDOS NetLoader - Upload programs to FPGC over raw Ethernet
"""

import socket
import struct
import sys
from time import sleep

ETH_TYPE_BDOS = 0xBD05
BDOS_PKT_DISCOVER = 0x01
BDOS_PKT_ANNOUNCE = 0x02
BDOS_PKT_PROGRAM = 0x20
BDOS_PKT_PROG_DATA = 0x21
BDOS_PKT_PROG_END = 0x22
BDOS_PKT_PROG_ACK = 0x23

CHUNK_SIZE = 1024

def discover_devices(interface, timeout=2):
    """Broadcast discover packet, collect responses"""
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, 
                        socket.htons(ETH_TYPE_BDOS))
    sock.bind((interface, 0))
    sock.settimeout(timeout)
    
    # Get local MAC
    local_mac = sock.getsockname()[4]
    
    # Build discover packet
    dst_mac = b'\xff\xff\xff\xff\xff\xff'  # Broadcast
    eth_hdr = dst_mac + local_mac + struct.pack('!H', ETH_TYPE_BDOS)
    bdos_hdr = struct.pack('BBHHH', 1, BDOS_PKT_DISCOVER, 0, 0, 0)
    
    sock.send(eth_hdr + bdos_hdr)
    
    devices = []
    while True:
        try:
            data = sock.recv(1518)
            if len(data) >= 14 + 8 + 32:
                eth_type = struct.unpack('!H', data[12:14])[0]
                if eth_type == ETH_TYPE_BDOS:
                    pkt_type = data[15]
                    if pkt_type == BDOS_PKT_ANNOUNCE:
                        mac = data[22:28]
                        name = data[30:46].rstrip(b'\x00').decode('ascii')
                        devices.append({'mac': mac, 'name': name})
        except socket.timeout:
            break
    
    sock.close()
    return devices

def upload_program(interface, target_mac, filepath, destname):
    """Upload a program file to target device"""
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW,
                        socket.htons(ETH_TYPE_BDOS))
    sock.bind((interface, 0))
    sock.settimeout(1)
    
    local_mac = sock.getsockname()[4]
    
    with open(filepath, 'rb') as f:
        data = f.read()
    
    total_size = len(data)
    print(f"Uploading {filepath} ({total_size} bytes) as {destname}")
    
    # Send program start
    eth_hdr = target_mac + local_mac + struct.pack('!H', ETH_TYPE_BDOS)
    bdos_hdr = struct.pack('BBHHH', 1, BDOS_PKT_PROGRAM, 0, 36, 0)
    payload = struct.pack('I', total_size) + destname.encode('ascii').ljust(32, b'\x00')
    sock.send(eth_hdr + bdos_hdr + payload)
    
    # Send chunks
    offset = 0
    seq = 1
    while offset < total_size:
        chunk = data[offset:offset + CHUNK_SIZE]
        chunk_len = len(chunk)
        
        bdos_hdr = struct.pack('BBHHH', 1, BDOS_PKT_PROG_DATA, seq, 8 + chunk_len, 0)
        payload = struct.pack('II', offset, chunk_len) + chunk
        sock.send(eth_hdr + bdos_hdr + payload)
        
        # Wait for ACK
        try:
            resp = sock.recv(1518)
            # Parse ACK...
        except socket.timeout:
            print(f"Timeout at offset {offset}, retrying...")
            continue
        
        offset += chunk_len
        seq += 1
        
        # Progress
        pct = (offset * 100) // total_size
        print(f"\r{pct}% complete", end='', flush=True)
    
    print("\nUpload complete!")
    sock.close()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: netloader.py <interface> [discover|upload <file> <dest>]")
        sys.exit(1)
    
    interface = sys.argv[1]
    
    if len(sys.argv) == 2 or sys.argv[2] == 'discover':
        print(f"Discovering BDOS devices on {interface}...")
        devices = discover_devices(interface)
        for d in devices:
            mac_str = ':'.join(f'{b:02x}' for b in d['mac'])
            print(f"  {d['name']} - {mac_str}")
```

### 11.2 NetHID Tool

A similar Python tool can send keyboard/mouse events to FPGC.

---

## 12. Security Considerations

### 12.1 No Security by Design

This raw Ethernet protocol has **no security**:
- No authentication
- No encryption
- Anyone on the LAN can discover and control devices

### 12.2 Mitigation

For controlled environments (lab/home), this is acceptable. For production:
- Use a physically isolated network
- Add simple shared-secret authentication in future version
- Consider VLANs for isolation

---

## 13. Implementation Checklist

- [ ] Implement ENC28J60 SPI driver
- [ ] Implement `enc_send_frame()` and `enc_recv_frame()`
- [ ] Define BDOS protocol headers
- [ ] Implement discovery request/response
- [ ] Implement NetLoader (kernel side)
- [ ] Implement NetHID event injection
- [ ] Create PC-side Python tools
- [ ] Implement raw Ethernet syscalls
- [ ] Test file upload with various sizes
- [ ] Test HID event latency
- [ ] Document protocol for future expansion

---

## 14. Summary

| Component | Implementation | Notes |
|-----------|----------------|-------|
| Protocol | Raw Ethernet | No IP/UDP/ARP |
| EtherType | 0xBD05 | Custom BDOS protocol |
| Packet format | BDOS header + payload | 8-byte header |
| NetLoader | Chunked upload | 1024-byte chunks with ACK |
| NetHID | Event packets | Keyboard/mouse events |
| Discovery | Broadcast/unicast | Find devices on LAN |
| Cluster | Data packets | Simple message passing |
| Security | None | Controlled environment only |
