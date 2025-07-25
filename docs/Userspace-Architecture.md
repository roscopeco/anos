# Userspace Architecture

> [!WARNING]
> This documentation is still **under construction** and **will** change as the system evolves.
>
> Some information in this document may represent future plans or ideas that may or may not be
> implemented. Direct code references represent a snapshot in time.
>
> It should not be taken and correct, complete, comprehensive or concrete until this warning is
> removed.

This document provides a comprehensive overview of Anos's userspace architecture, explaining how the microkernel's minimal design enables a rich ecosystem of userspace services and applications.

## Architectural Overview

Anos implements a true microkernel architecture where the kernel provides only essential services:
- **Physical and virtual memory management**
- **Task scheduling and SMP coordination**
- **Inter-process communication (IPC)**
- **Basic hardware drivers** (timers, interrupt controllers)

All operating system functionality is implemented in userspace through a hierarchy of servers and services.

## Component Hierarchy

```
┌─────────────────────────────────────────────────────────┐
│                        KERNEL                           │
│  Memory Management │ Scheduler │ IPC │ Basic Drivers    │
└─────────────┬───────────────────────────────────────────┘
              │
    ┌─────────▼─────────┐
    │      SYSTEM       │ ◄─── User-mode Supervisor
    │ ELF Loader │ VFS  │
    │ Process Mgr│ RAMFS│
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │      DEVMAN       │ ◄─── Hardware Discovery
    │ ACPI │ Driver Mgr │
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │   DEVICE DRIVERS  │ ◄─── Hardware Abstraction
    │ PCIDRV │ AHCIDRV  │
    │  ...   │   ...    │
    └─────────┬─────────┘
              │
    ┌─────────▼─────────┐
    │   APPLICATIONS    │ ◄─── User Software
    │  Shells │ Utils   │
    │  Games  │ Servers │
    └───────────────────┘
```

## Core Components

### SYSTEM - The User-mode Supervisor

**Role**: SYSTEM acts as the privileged user-mode supervisor, bridging the minimal microkernel with higher-level OS services.

**Architecture**:
- **Multi-threaded design** with dedicated service threads
- **Comprehensive capabilities** - receives nearly all kernel syscall capabilities
- **Service provider** - exposes named IPC channels for system services
- **Process factory** - responsible for creating all other userspace processes

**Core Services**:
1. **Virtual File System (VFS)** - `SYSTEM::VFS` channel
   - Filesystem driver lookup and routing
   - Mount point management
   - Currently supports boot RAMFS exclusively

2. **Process Management** - `SYSTEM::PROCESS` channel  
   - ELF binary loading and process creation
   - Capability delegation to child processes
   - Process lifecycle management

3. **RAMFS Driver** - Internal unnamed channel
   - File operations on embedded boot filesystem
   - Zero-copy file data delivery
   - Path resolution and file lookup

**Key Implementation Details**:
```c
// Service thread architecture
vfs_thread()         // IPC buffer: 0xa0000000
ramfs_driver_thread() // IPC buffer: 0xb0000000  
process_manager_thread() // IPC buffer: 0xc0000000
```

**Memory Layout**:
- Code base: `0x01000000`
- Stack: `0x43fff000` (1MB, growing downward)
- Service buffers: Fixed addresses per service

### DEVMAN - Hardware Discovery and Driver Management

**Role**: DEVMAN orchestrates hardware discovery and driver initialization using ACPI-based enumeration.

**Architecture**:
- **Single-threaded** initialization service
- **One-shot execution** - performs discovery then sleeps
- **Capability-based driver spawning** - delegates minimal privileges

**Hardware Discovery Process**:
1. **ACPI Table Access** - maps firmware tables using `anos_map_firmware_tables()`
2. **RSDP Processing** - follows Root System Description Pointer chain
3. **MCFG Parsing** - discovers PCI Express configuration spaces
4. **Bus Driver Spawning** - launches PCI drivers per host bridge

**Driver Spawning Pattern**:
```c
const char *pci_argv[] = {
    "boot:/pcidrv.elf",
    ecam_base_str,     // Physical base address
    segment_str,       // PCI segment number  
    bus_start_str,     // Starting bus number
    bus_end_str        // Ending bus number
};

spawn_process_via_system(0x100000, 9, pci_caps, 5, pci_argv);
```

### Device Driver Ecosystem

**Architecture Pattern**: Anos device drivers follow a **hierarchical enumeration model**:

```
DEVMAN (ACPI Discovery)
└── PCIDRV (Bus Driver)
    ├── AHCIDRV (SATA Controller)
    ├── NVMEDRV (NVMe Controller) [Future]
    └── NETDRV (Network Controller) [Future]
```

#### PCI Bus Driver (PCIDRV)

**Role**: Enumerates PCI Express devices within assigned bus ranges and spawns device-specific drivers.

**Key Features**:
- **ECAM Mapping** - maps PCI Express configuration space  
- **Device Enumeration** - scans all possible device locations
- **Vendor/Device ID lookup** - identifies hardware types
- **Driver Matching** - spawns appropriate device drivers

**Enumeration Algorithm**:
```c
// Three-level enumeration: Bus → Device → Function
for (bus = bus_start; bus <= bus_end; bus++) {
    for (device = 0; device < 32; device++) {
        for (function = 0; function < 8; function++) {
            // Check vendor ID for device presence
            // Spawn driver if recognized hardware found
        }
    }
}
```

#### AHCI Driver (AHCIDRV)

**Role**: Manages SATA controllers using the Advanced Host Controller Interface specification.

**Architecture**:
- **Controller abstraction** - single controller per driver instance
- **Port management** - individual port state tracking
- **Command processing** - AHCI command list/table management
- **Persistent service** - remains active for storage operations

**Hardware Interface**:
- **MMIO Register Access** - memory-mapped hardware registers
- **DMA Operations** - direct memory access for data transfer  
- **Interrupt Handling** - completion notification processing
- **IDENTIFY Command** - drive capability discovery

## Inter-Component Communication

### IPC Architecture

Anos uses **synchronous message-passing IPC** as the primary communication mechanism:

**Channel Types**:
1. **Named Channels** - globally discoverable services
2. **Anonymous Channels** - direct peer-to-peer communication
3. **Capability Channels** - secure channel delegation

**Communication Patterns**:

#### Service Discovery Pattern
```c
// Standard service lookup
uint64_t vfs_channel = anos_find_named_channel("SYSTEM::VFS");
uint64_t proc_channel = anos_find_named_channel("SYSTEM::PROCESS");
```

#### Request-Reply Pattern  
```c
// Synchronous service request
uint64_t result = anos_send_message(channel, tag, size, buffer);
// Sender blocks until service processes request and replies
```

#### Service Provider Pattern
```c
// Service thread loop
while (1) {
    uint64_t msg_cookie = anos_recv_message(channel, &tag, &size, buffer);
    // Process request based on tag...
    anos_reply_message(msg_cookie, result);
}
```

### Message Protocols

#### VFS Protocol (`SYSTEM::VFS`)
```c
#define VFS_FIND_FS_DRIVER (1)

// Request: mount prefix (e.g., "boot:")
// Response: filesystem driver channel ID
```

#### Process Management Protocol (`SYSTEM::PROCESS`)
```c
#define PROCESS_SPAWN (1)

typedef struct {
    uint64_t stack_size;  // New process stack size
    uint16_t argc;        // Argument count  
    uint16_t capc;        // Capability count
    char data[];          // Capabilities + argv strings
} ProcessSpawnRequest;

// Response: Process ID (positive) or error code (negative)
```

#### RAMFS Protocol (Internal Channel)
```c
#define FS_QUERY_OBJECT_SIZE (1)
#define FS_LOAD_OBJECT_PAGE (2)

typedef struct {
    uint64_t start_byte_ofs;  // File offset
    char name[];              // File path
} FileLoadPageQuery;

// Response: File size or file data page
```

## Security Architecture

### Capability-Based Security Model

**Principle**: Every operation requires explicit capability tokens. No ambient authority exists.

**Capability Flow**:
```
Kernel (Full Hardware Access)
    ↓ delegates comprehensive capabilities
SYSTEM (Userspace Supervisor)  
    ↓ delegates hardware + process creation
DEVMAN (Hardware Discovery)
    ↓ delegates hardware access only
Device Drivers (Minimal Hardware Access)
```

**Capability Types**:
- **Memory Management**: `SYSCALL_ID_MAP_VIRTUAL`, `SYSCALL_ID_MAP_PHYSICAL`
- **Process Control**: `SYSCALL_ID_CREATE_PROCESS`, `SYSCALL_ID_CREATE_THREAD`
- **IPC Operations**: `SYSCALL_ID_SEND_MESSAGE`, `SYSCALL_ID_RECV_MESSAGE`
- **Hardware Access**: `SYSCALL_ID_MAP_FIRMWARE_TABLES`, `SYSCALL_ID_MAP_PHYSICAL`
- **Debug Output**: `SYSCALL_ID_DEBUG_PRINT`, `SYSCALL_ID_DEBUG_CHAR`

### Process Isolation

**Memory Isolation**:
- Each process has isolated virtual address space
- No shared memory except through explicit IPC
- Page-level access control via kernel MMU management

**Capability Isolation**:
- Capabilities are cryptographically random 64-bit tokens
- Invalid usage triggers escalating delays (anti-bruteforce)
- Capabilities cannot be forged or guessed

**IPC Isolation**:
- All IPC requires channel capability tokens
- Messages are automatically validated and sanitized
- Zero-copy transfers use secure page table manipulation

## File System Architecture

### Virtual File System (VFS)

**Design**: SYSTEM implements a routing-based VFS that delegates to filesystem drivers.

**Mount Point System**:
- **Prefix-based routing**: `"mount:path"` format
- **Current mounts**: Only `"boot:"` supported (RAMFS)
- **Driver lookup**: VFS returns appropriate filesystem driver channel

**File Access Flow**:
1. **Application** requests file via `"boot:/filename"`
2. **VFS** parses prefix, returns RAMFS driver channel
3. **Application** communicates directly with RAMFS driver
4. **RAMFS** provides file size and data pages

### RAMFS Implementation

**Architecture**: Embedded read-only filesystem built into kernel image.

**Data Structures**:
```c
typedef struct {
    uint32_t magic;        // 0x41524653 ("ARFS")
    uint32_t version;      // Format version
    uint32_t size;         // Total filesystem size
    uint32_t file_count;   // Number of files
    // ... additional metadata
} AnosRAMFSHeader;

typedef struct {
    uint32_t offset;       // Offset from FS start
    uint32_t length;       // File size in bytes
    char name[16];         // Filename (null-terminated)
    // ... additional metadata
} AnosRAMFSFileHeader;
```

**File Operations**:
- **Linear search**: Files located via sequential header scan
- **Zero-copy delivery**: File data returned via page mapping
- **Read-only access**: No write operations supported

## Application Development Model

### Process Creation Model

**No fork/exec**: Anos doesn't support POSIX process creation. Instead:

```c
// Application spawning via SYSTEM::PROCESS
ProcessSpawnRequest request = {
    .stack_size = 0x100000,  // 1MB stack
    .argc = 2,
    .capc = 3,
    .data = { /* capabilities + argv strings */ }
};

int64_t pid = anos_send_message(proc_channel, PROCESS_SPAWN, 
                               sizeof(request), &request);
```

### Service Development Pattern

**Standard Server Pattern**:
1. **Initialization** - hardware setup, resource allocation
2. **Service Registration** - register named channel (optional)
3. **Service Loop** - process requests via IPC
4. **Cleanup** - resource deallocation on shutdown

**Example Service Structure**:
```c
int main(int argc, char *argv[]) {
    // Parse command line arguments
    // Initialize hardware/resources
    // Create IPC channel
    // Register service name (optional)
    
    while (1) {
        uint64_t msg_cookie = anos_recv_message(channel, &tag, &size, buffer);
        
        switch (tag) {
        case SERVICE_REQUEST_1:
            // Handle request type 1
            break;
        case SERVICE_REQUEST_2:  
            // Handle request type 2
            break;
        }
        
        anos_reply_message(msg_cookie, result);
    }
}
```

### Library Support

**System Library**: Applications link against Anos-specific libraries providing:
- **Wrapper functions** for kernel syscalls
- **IPC helper functions** for common communication patterns  
- **Service discovery utilities** for finding named channels
- **Protocol implementations** for standard services

## Performance Characteristics

### IPC Performance

**Zero-Copy Design**: Large data transfers use page table manipulation rather than copying:
- **Small messages**: Copied to/from kernel buffers
- **Large messages**: Physical pages mapped between address spaces
- **Maximum message size**: 4KB (one page)

**Synchronous Model**: All IPC operations block until completion:
- **Latency**: ~1-2 microseconds for local IPC
- **Throughput**: Limited by synchronous model
- **Scalability**: Good for request-reply patterns

### Memory Efficiency

**Minimal Kernel**: Kernel footprint ~500KB including all drivers
**Process Overhead**: ~256KB minimum per process (stack + metadata)
**Shared Resources**: IPC channels and capabilities minimize duplication

### Scalability Characteristics

**SMP Support**: Full multi-core support with per-core scheduling
**Process Limit**: Limited by available memory (~4000 processes per GB)
**IPC Scalability**: Channels scale to thousands of concurrent operations

## Current Limitations and Future Directions

### Current Limitations

1. **Limited Filesystem Support**: Only RAMFS currently implemented
2. **No Networking**: Network stack not yet implemented  
3. **Basic Driver Set**: Only PCI/AHCI drivers available
4. **No GUI**: Graphical interface not implemented
5. **RISC-V SMP**: RISC-V port currently single-core only

### Planned Extensions

1. **Ext4/FAT32 Support**: Additional filesystem drivers
2. **Network Stack**: TCP/IP implementation with driver support
3. **USB Support**: USB bus and device driver framework
4. **Graphics Drivers**: Display and input device support
5. **Application Framework**: Higher-level development libraries

### Design Evolution

**Microkernel Maturity**: The architecture successfully demonstrates microkernel principles while maintaining performance and security.

**Service Ecosystem**: The hierarchical service model scales well and enables modular development.

**Security Model**: Capability-based security provides strong isolation without performance penalties.

This userspace architecture demonstrates how a microkernel can provide a robust foundation for complex operating system functionality while maintaining strict security boundaries and enabling modular development of system services.