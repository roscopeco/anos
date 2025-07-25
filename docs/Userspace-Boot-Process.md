# Userspace Boot Process

> [!WARNING]
> This documentation is still **under construction** and **will** change as the system evolves.
> 
> Some information in this document may represent future plans or ideas that may or may not be 
> implemented. Direct code references represent a snapshot in time.
> 
> It should not be taken and correct, complete, comprehensive or concrete until this warning is 
> removed.

This document describes how Anos transitions from kernel initialization to a fully operational userspace environment, including the loading of SYSTEM and the server ecosystem.

## Overview

Anos follows a microkernel architecture where the kernel provides only essential services (memory management, scheduling, IPC, and basic drivers), while all OS functionality is implemented in userspace. The boot process follows this sequence:

```
Kernel → SYSTEM → DEVMAN → Device Drivers → Application Services
```

The transition involves several distinct phases, each with specific responsibilities and dependencies.

## Phase 1: Kernel to SYSTEM Transition

### Kernel Completion
Before userspace initialization begins, the kernel completes its essential setup:
- Physical and virtual memory management initialization
- Scheduler and SMP (multi-core) setup  
- IPC channel infrastructure
- Basic hardware driver initialization (timers, interrupt controllers)
- RAMFS embedding and validation

**Key Code References:**
- Kernel initialization: `kernel/entrypoint.c`
- RAMFS loading: `kernel/system.c:system_init()`

### SYSTEM Loading
The kernel directly loads SYSTEM as the first userspace process:

1. **Binary Loading**: SYSTEM is embedded in kernel image as a flat binary
2. **Process Creation**: Kernel creates first user process using `process_create_initial()`
3. **Capability Grant**: SYSTEM receives comprehensive kernel capabilities for system management
4. **Control Transfer**: Kernel jumps to SYSTEM entry point with prepared stack

**Memory Layout for SYSTEM:**
- Virtual Address: `0x01000000` (standard userspace code base)
- Stack: `0x43fff000` (growing downward from `0x44000000`)
- Capabilities: Full access to all kernel syscalls
- Arguments: None (argc=0, argv=NULL)

**Key Code References:**
- SYSTEM loading: `kernel/system.c:load_system()`
- Initial process creation: `kernel/process/process.c:process_create_initial()`

## Phase 2: SYSTEM Initialization

### Bootstrap Sequence
SYSTEM performs critical userspace initialization in `system/main.c:main()`:

1. **Memory Information**: Reports physical memory usage via `anos_get_mem_info()`
2. **RAMFS Validation**: Optionally dumps embedded RAMFS for debugging
3. **Capability Acquisition**: Receives comprehensive kernel capabilities including:
   - Debug output: `SYSCALL_ID_DEBUG_PRINT`, `SYSCALL_ID_DEBUG_CHAR`
   - Memory management: `SYSCALL_ID_MAP_VIRTUAL`, `SYSCALL_ID_ALLOC_PHYSICAL_PAGES`
   - Process control: `SYSCALL_ID_CREATE_PROCESS`, `SYSCALL_ID_CREATE_THREAD`
   - IPC operations: `SYSCALL_ID_SEND_MESSAGE`, `SYSCALL_ID_RECV_MESSAGE`
   - Hardware access: `SYSCALL_ID_MAP_FIRMWARE_TABLES`, `SYSCALL_ID_MAP_PHYSICAL`

### Service Infrastructure Setup
SYSTEM establishes the core service infrastructure:

1. **IPC Channel Creation**:
   - `vfs_channel` - Virtual File System services
   - `ramfs_channel` - RAMFS file operations
   - `process_manager_channel` - Process creation services

2. **Named Service Registration**:
   - `"SYSTEM::VFS"` - File system lookup service
   - `"SYSTEM::PROCESS"` - Process management service

3. **Service Thread Creation**:
   - VFS service thread with 1MB stack
   - RAMFS driver thread with 256KB stack  
   - Process manager thread with 256KB stack

### Service Thread Architecture
Each service runs in a dedicated thread with fixed IPC buffer addresses:

```c
// VFS thread - handles filesystem driver lookup
static void *vfs_thread(void *arg) {
    // IPC buffer at 0xa0000000
    while (1) {
        uint64_t message_cookie = anos_recv_message(vfs_channel, &tag, &size, (void*)0xa0000000);
        // Process VFS requests...
        anos_reply_message(message_cookie, result);
    }
}

// RAMFS thread - handles file operations
static void *ramfs_driver_thread(void *arg) {
    // IPC buffer at 0xb0000000
    while (1) {
        uint64_t message_cookie = anos_recv_message(ramfs_channel, &tag, &size, (void*)0xb0000000);
        // Process file requests...
        anos_reply_message(message_cookie, result);
    }
}

// Process manager thread - handles process creation
static void *process_manager_thread(void *arg) {
    // IPC buffer at 0xc0000000
    while (1) {
        uint64_t message_cookie = anos_recv_message(process_manager_channel, &tag, &size, (void*)0xc0000000);
        // Process spawn requests...
        anos_reply_message(message_cookie, process_id);
    }
}
```

**Key Code References:**
- Service setup: `system/main.c:431-451`
- VFS thread: `system/main.c:474-515`
- RAMFS thread: `system/main.c:306-344`  
- Process manager: `system/main.c:270-304`

## Phase 3: Initial Server Startup

### DEVMAN Launch
SYSTEM immediately launches the device manager as the first server:

```c
const char *devman_argv[] = {"boot:/devman.elf"};
const int64_t devman_pid = create_server_process(
    0x100000,        // 1MB stack
    10,              // 10 capabilities
    new_process_caps, // Capability array
    1,               // argc = 1
    devman_argv      // argv
);
```

**DEVMAN Capabilities:**
DEVMAN receives a comprehensive set of capabilities for hardware management:
- Debug output and basic operations
- Physical memory mapping (`SYSCALL_ID_MAP_PHYSICAL`)
- Virtual memory management (`SYSCALL_ID_MAP_VIRTUAL`) 
- Process creation (`SYSCALL_ID_CREATE_PROCESS`)
- IPC operations for communication
- Hardware access (`SYSCALL_ID_MAP_FIRMWARE_TABLES`)

### Process Creation Mechanism
The `create_server_process()` function implements sophisticated process creation:

1. **Memory Setup**: Maps SYSTEM code into new process address space temporarily
2. **Stack Preparation**: Constructs initial stack with capabilities and arguments
3. **Process Creation**: Uses `anos_create_process()` with trampoline entry point
4. **ELF Loading Trampoline**: New process starts with SYSTEM code, loads target ELF, then jumps to it

**Key Code References:**
- Process creation: `system/process.c:create_server_process()`
- Stack setup: `system/process.c:build_new_process_init_values()`
- Trampoline: `system/arch/x86_64/trampolines.asm`

## Phase 4: Hardware Discovery and Driver Loading

### DEVMAN Hardware Discovery
DEVMAN performs system-wide hardware discovery:

1. **ACPI Initialization**: Maps and parses ACPI firmware tables
2. **Hardware Enumeration**: Discovers PCI Express host bridges via MCFG table
3. **Driver Spawning**: Launches PCI bus drivers for each discovered host bridge

### Driver Hierarchy Establishment
DEVMAN establishes the driver hierarchy:

```
DEVMAN (Hardware Discovery)
├── PCIDRV (Segment 0, Bus 0-255)
├── PCIDRV (Segment 1, Bus 0-255)
└── ... (Additional PCI segments)
    └── AHCIDRV (Per AHCI Controller)
        ├── AHCI Port 0
        ├── AHCI Port 1
        └── ... (Per Port)
```

### PCI Driver Operation
Each PCI driver performs:

1. **ECAM Mapping**: Maps PCI Express configuration space
2. **Device Enumeration**: Scans all devices in assigned bus range
3. **Device-Specific Drivers**: Spawns specialized drivers (AHCI, etc.)
4. **Service Completion**: Exits after enumeration (task-based lifecycle)

**Key Code References:**
- DEVMAN hardware discovery: `servers/devman/main.c:map_and_init_acpi()`  
- PCI driver spawning: `servers/devman/main.c:spawn_pci_bus_driver()`
- PCI enumeration: `servers/pcidrv/enumerate.c`

## Phase 5: Service Readiness

### System Service Availability
After the boot process completes, the following services are available:

**Core Services (SYSTEM):**
- `SYSTEM::VFS` - File system abstraction and driver lookup
- `SYSTEM::PROCESS` - Process creation and management
- RAMFS driver - Boot filesystem access

**Hardware Services:**
- PCI bus drivers - Hardware enumeration and management
- AHCI drivers - SATA storage controller access
- Device-specific drivers - Per-hardware functionality

### Service Discovery Model
Applications and servers discover services using the named channel system:

```c
uint64_t vfs_channel = anos_find_named_channel("SYSTEM::VFS");
uint64_t process_channel = anos_find_named_channel("SYSTEM::PROCESS");
```

The naming convention follows: `"<SERVICE_PROVIDER>::<SERVICE_TYPE>"`

## Boot Process Timeline

| Phase | Component | Duration | Key Activities |
|-------|-----------|----------|----------------|
| 0 | Kernel | ~100ms | Memory setup, SMP init, driver init |
| 1 | SYSTEM | ~10ms | Service setup, thread creation |
| 2 | DEVMAN | ~50ms | ACPI parsing, hardware discovery |
| 3 | PCI Drivers | ~20ms | PCI enumeration, device discovery |
| 4 | Device Drivers | ~30ms | Hardware initialization |
| 5 | Service Ready | - | Full userspace operational |

**Total Boot Time: ~210ms** (typical on modern hardware)

## Security Model During Boot

### Capability Flow
The boot process implements strict capability-based security:

1. **Kernel → SYSTEM**: Full capabilities for system management
2. **SYSTEM → DEVMAN**: Hardware access and process creation capabilities  
3. **DEVMAN → PCI Drivers**: Hardware access and limited process creation
4. **PCI Drivers → Device Drivers**: Device-specific hardware access only

### Privilege Separation
Each component receives only the minimum capabilities required:
- **Kernel**: Full hardware access, memory management
- **SYSTEM**: Userspace supervisor with delegation rights
- **DEVMAN**: Hardware discovery and driver management
- **Drivers**: Device-specific hardware access only

### Trust Boundaries
- **Kernel**: Trusted computing base (TCB)
- **SYSTEM**: Trusted supervisor (part of TCB)
- **DEVMAN**: Semi-privileged (hardware management only)
- **Drivers**: Untrusted (isolated, capability-limited)

## Error Handling and Recovery

### Boot Failure Scenarios
The boot process includes error handling for common failures:

1. **SYSTEM Failure**: Kernel panic (system cannot continue)
2. **DEVMAN Failure**: No hardware drivers loaded (minimal system)
3. **Driver Failures**: Specific hardware unavailable (graceful degradation)
4. **Service Failures**: Individual services unavailable (partial functionality)

### Diagnostic Information
Boot process provides extensive diagnostic output:
- Memory usage reports
- Hardware discovery logs  
- Driver loading status
- Service availability confirmation

**Debug Output References:**
- SYSTEM: `system/main.c:354-376`
- DEVMAN: `servers/devman/main.c:575-595`
- Drivers: Throughout respective main.c files

This boot process design ensures a secure, modular, and reliable transition from kernel to fully operational userspace, establishing the foundation for Anos's microkernel architecture while maintaining strict security boundaries through capability-based access control.