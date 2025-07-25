# Userspace IPC and Communication Patterns

> [!WARNING]
> This documentation is still **under construction** and **will** change as the system evolves.
>
> Some information in this document may represent future plans or ideas that may or may not be
> implemented. Direct code references represent a snapshot in time.
>
> It should not be taken and correct, complete, comprehensive or concrete until this warning is
> removed.

> [!NOTE]
> This document focuses on the specifics of the IPC messaging used by the user-mode supervisor
> and related systems. This section represents a high-level overview only - for more in-depth
> treatment of the kernel's IPC system, see [Kernel IPC](./IPC.md).

Anos implements a **synchronous message-passing IPC system** built on capability-based security. All userspace
communication flows through the kernel's IPC subsystem, which provides strong isolation while enabling efficient 
zero-copy data transfer.

This document provides technical documentation of the communication patterns used throughout the userspace ecosystem.

### Namespace Organization

The current namespace follows hierarchical naming conventions:

| Service Name          | Provider | Purpose                         | Access Pattern     |
|-----------------------|----------|---------------------------------|--------------------|
| **`SYSTEM::VFS`**     | SYSTEM   | File system driver lookup       | Public service     |
| **`SYSTEM::PROCESS`** | SYSTEM   | Process creation and management | Public service     |
| *(RAMFS channel)*     | SYSTEM   | Boot filesystem operations      | Internal/delegated |

**Namespace Design**:
- **Provider Prefix**: `"<SERVICE_PROVIDER>::<SERVICE_TYPE>"`
- **Global Scope**: All named channels are globally accessible
- **Capability Control**: Access still requires channel capability tokens
- **Hash-Based Lookup**: Uses SDBM hash algorithm for efficient lookups

### Implementation Details

```c
// kernel/ipc/named.c - Named channel storage
static HashTable *name_table; // Maps name hash → channel cookie

// Registration
bool named_channel_register(uint64_t cookie, char *name) {
    uint64_t name_hash = str_hash_sdbm((unsigned char *)name, 255);
    return hashtable_put(name_table, name_hash, (void*)cookie);
}

// Discovery  
uint64_t named_channel_find(char *name) {
    uint64_t name_hash = str_hash_sdbm((unsigned char *)name, 255);
    return (uint64_t)hashtable_get(name_table, name_hash);
}
```

## Core Communication Patterns

### 1. Service Request-Reply Pattern

**Usage**: Standard pattern for accessing system services.

**Client Side**:
```c
// Prepare request data
ServiceRequest request = {
    .operation = SERVICE_OP_QUERY,
    .parameter = some_value,
    // ... additional fields
};

// Send request and get reply  
uint64_t result = anos_send_message(service_channel, SERVICE_OP_QUERY,
                                   sizeof(request), &request);

if (result > 0) {
    // Success - result contains service response
} else {
    // Error - result contains error code
}
```

**Server Side**:
```c
// Service thread loop
while (1) {
    ServiceRequest request;
    uint64_t tag, size;
    
    // Receive request (blocks until client sends)
    uint64_t msg_cookie = anos_recv_message(service_channel, &tag, &size, &request);
    
    // Process request based on opcode
    uint64_t result;
    switch (tag) {
    case SERVICE_OP_QUERY:
        result = process_query_operation(&request);
        break;
    case SERVICE_OP_UPDATE:
        result = process_update_operation(&request);
        break;
    default:
        result = ERROR_UNKNOWN_OPERATION;
    }
    
    // Send reply (unblocks client)
    anos_reply_message(msg_cookie, result);
}
```

### 2. Process Spawning Pattern

**Usage**: Creating new processes via SYSTEM::PROCESS service.

**Message Structure**:
```c
// system/main.c - Process creation protocol
typedef struct {
    uint64_t stack_size;       // New process stack size
    uint16_t argc;             // Command line argument count
    uint16_t capc;             // Capability count to delegate
    char data[];               // Variable-length payload:
                              //   - InitCapability[capc]
                              //   - argv strings (null-terminated)
} ProcessSpawnRequest;

typedef struct {
    uint64_t capability_id;    // Syscall ID constant
    uint64_t capability_cookie; // Actual capability token
} InitCapability;
```

**Client Usage**:
```c
// devman/main.c - Example: DEVMAN spawning PCI driver
const InitCapability pci_caps[] = {
    {.capability_id = SYSCALL_ID_DEBUG_PRINT, 
     .capability_cookie = __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
    {.capability_id = SYSCALL_ID_MAP_PHYSICAL,
     .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
    // ... additional capabilities
};

const char *pci_argv[] = {
    "boot:/pcidrv.elf",
    "0x80000000",        // ECAM base address
    "0",                 // PCI segment
    "0",                 // Start bus  
    "255"                // End bus
};

// Build request with capabilities and arguments
ProcessSpawnRequest *request = build_spawn_request(
    0x100000,           // 1MB stack
    5,                  // argc
    sizeof(pci_caps)/sizeof(pci_caps[0]), // capc  
    pci_caps,
    pci_argv
);

// Send spawn request
int64_t pid = anos_send_message(process_channel, PROCESS_SPAWN,
                               request_size, request);

if (pid > 0) {
    printf("Process spawned with PID %ld\n", pid);
} else {
    printf("Process spawn failed: %ld\n", pid);
}
```

### 3. File System Access Pattern

**Usage**: Multi-step file access via VFS and filesystem drivers.

**Step 1: VFS Driver Lookup**
```c
// Client requests filesystem driver for path
char path[] = "boot:/filename.ext";
uint64_t fs_channel = anos_send_message(vfs_channel, VFS_FIND_FS_DRIVER,
                                        strlen(path) + 1, path);

if (fs_channel == 0) {
    // Filesystem not found or not mounted
    return ERROR_FILE_NOT_FOUND;
}
```

**Step 2: File Operations**
```c  
// Query file size
char filename[] = "/filename.ext";
uint64_t file_size = anos_send_message(fs_channel, FS_QUERY_OBJECT_SIZE,
                                      strlen(filename) + 1, filename);

// Load file data (page-aligned buffer required)
static char __attribute__((aligned(4096))) file_buffer[4096];
FileLoadPageQuery query = {
    .start_byte_ofs = 0,
    // filename copied after struct
};
strcpy(query.name, filename);

uint64_t bytes_read = anos_send_message(fs_channel, FS_LOAD_OBJECT_PAGE,
                                       sizeof(query) + strlen(filename) + 1,
                                       &query);
```

### 4. Hardware Driver Communication Pattern

**Usage**: Device enumeration and driver coordination.

**PCI Enumeration Example**:
```c
// pcidrv/enumerate.c - Device discovery triggers driver spawning
static void check_ahci_controller(const PCIBusDriver *bus_driver,
                                 uint8_t bus, uint8_t device, uint8_t function) {
    // Read PCI configuration space
    uint32_t vendor_device = pci_config_read32(bus_driver, bus, device, function, 0);
    
    if (vendor_device == INTEL_ICH9_AHCI_ID) {
        // Get BAR5 (AHCI base address)
        uint64_t ahci_base = pci_config_read32(bus_driver, bus, device, function, 0x24);
        
        // Spawn AHCI driver for this controller
        spawn_ahci_driver(ahci_base);
    }
}

static void spawn_ahci_driver(uint64_t ahci_base) {
    const char *ahci_argv[] = {
        "boot:/ahcidrv.elf",
        ahci_base_string
    };
    
    // Use SYSTEM::PROCESS to create AHCI driver
    int64_t pid = spawn_process_via_system(0x100000, ahci_caps, 2, ahci_argv);
}
```

## Service-Specific Protocols

### SYSTEM::VFS Protocol

**Operations**:
```c
#define VFS_FIND_FS_DRIVER (1)

// Request: Mount prefix string (e.g., "boot:")
// Response: Filesystem driver channel ID (or 0 if not found)
```

**Implementation** (`system/main.c:474-515`):
```c
static void *vfs_thread(void *arg) {
    while (1) {
        uint64_t tag, size;
        uint64_t message_cookie = anos_recv_message(vfs_channel, &tag, &size,
                                                   (void*)0xa0000000);
        
        uint64_t result = 0;
        switch (tag) {
        case VFS_FIND_FS_DRIVER:
            char *path = (char*)0xa0000000;
            if (strncmp(path, "boot:", 5) == 0) {
                result = ramfs_channel; // Return RAMFS driver channel
            }
            // Future: Add support for other mount points
            break;
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

### SYSTEM::PROCESS Protocol

**Operations**:
```c
#define PROCESS_SPAWN (1)

typedef struct {
    uint64_t stack_size;    // Stack size for new process
    uint16_t argc;          // Number of command line arguments
    uint16_t capc;          // Number of capabilities to delegate
    char data[];            // Packed capability array + argv strings
} ProcessSpawnRequest;
```

**Implementation** (`system/main.c:270-304`):
```c
static void *process_manager_thread(void *arg) {
    while (1) {
        uint64_t tag, size;
        uint64_t message_cookie = anos_recv_message(process_manager_channel, &tag, &size,
                                                   (void*)0xc0000000);
        
        int64_t result = -1;
        switch (tag) {
        case PROCESS_SPAWN:
            ProcessSpawnRequest *req = (ProcessSpawnRequest*)0xc0000000;
            
            // Validate request parameters
            if (req->argc <= 64 && req->capc <= 32) {
                // Extract capabilities and arguments from packed data
                InitCapability *caps = (InitCapability*)req->data;
                char **argv = extract_argv_from_packed_data(req);
                
                // Create new process
                result = create_server_process(req->stack_size, req->capc, caps,
                                             req->argc, argv);
            }
            break;
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

### RAMFS Driver Protocol

**Operations**:
```c
#define FS_QUERY_OBJECT_SIZE (1)
#define FS_LOAD_OBJECT_PAGE (2)

typedef struct {
    uint64_t start_byte_ofs;    // Starting byte offset in file
    char name[];                // File path (relative to mount)
} FileLoadPageQuery;
```

**Implementation** (`system/main.c:306-344`):
```c
static void *ramfs_driver_thread(void *arg) {
    while (1) {
        uint64_t tag, size;
        uint64_t message_cookie = anos_recv_message(ramfs_channel, &tag, &size,
                                                   (void*)0xb0000000);
        
        uint64_t result = 0;
        switch (tag) {
        case FS_QUERY_OBJECT_SIZE:
            char *filename = (char*)0xb0000000;
            const AnosRAMFSFileHeader *file = ramfs_find_file(filename);
            result = file ? file->length : 0;
            break;
            
        case FS_LOAD_OBJECT_PAGE:
            FileLoadPageQuery *query = (FileLoadPageQuery*)0xb0000000;
            const AnosRAMFSFileHeader *file = ramfs_find_file(query->name);
            if (file) {
                // Zero-copy return: map file data page to sender's buffer
                void *file_data = ramfs_file_open(file);
                memcpy((void*)0xb0000000, (char*)file_data + query->start_byte_ofs,
                       min(4096, file->length - query->start_byte_ofs));
                result = min(4096, file->length - query->start_byte_ofs);
            }
            break;
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

## Advanced Communication Patterns

### 1. Channel Delegation Pattern

**Usage**: Services can delegate channel access to clients for direct communication.

```c
// Service creates dedicated channel for client
uint64_t client_channel = anos_create_channel();

// Service provides channel capability to client via reply
anos_reply_message(message_cookie, client_channel);

// Client can now communicate directly without going through service
uint64_t result = anos_send_message(client_channel, operation, size, data);
```

### 3. Pipeline Communication Pattern

**Usage**: Chained processing across multiple services.

```c
// Client → Service A → Service B → Service C → Client
uint64_t step1_result = anos_send_message(service_a_channel, PROCESS_STEP1, 
                                         input_size, input_data);

uint64_t step2_result = anos_send_message(service_b_channel, PROCESS_STEP2,
                                         sizeof(step1_result), &step1_result);

uint64_t final_result = anos_send_message(service_c_channel, PROCESS_FINAL,
                                         sizeof(step2_result), &step2_result);
```

## Performance Characteristics

> [!WARNING]
> Numbers in this section are aspirational, and do not represent tested values on
> any platform.
> 
> They should not be taken and correct, complete, comprehensive or concrete until 
> this warning is removed.

### Latency Analysis

**Local IPC Latency**: ~1-2 microseconds per message (typical modern hardware)
- **Send syscall**: ~300ns (context switch + queue operation)  
- **Receive syscall**: ~300ns (dequeue + buffer mapping)
- **Reply syscall**: ~300ns (cleanup + sender wakeup)
- **User processing**: Variable (depends on service complexity)

**Zero-Copy Benefits**:
- **Small messages** (≤64 bytes): No significant benefit due to syscall overhead
- **Medium messages** (256 bytes - 4KB): 2-3x faster than copying
- **Large messages** (4KB): 5-10x faster than copying

### Throughput Analysis

**Synchronous Limitations**: 
- **Maximum rate**: ~500,000 messages/second per channel (single-threaded)
- **Multi-channel scaling**: Linear with number of service threads
- **Bottlenecks**: Context switching and memory management overhead

**Optimization Strategies**:
- **Batch operations**: Pack multiple requests into single message  
- **Cache locality**: Keep frequently-used data in mapped buffers
- **Service threading**: Use multiple service threads for CPU-bound operations

### Memory Usage

**Channel Overhead**: ~256 bytes per channel (queue structures + locks)
**Message Overhead**: ~128 bytes per in-flight message (temporary allocation)
**Buffer Requirements**: 4KB aligned buffers (significant for small messages)

## Security Properties

### Capability-Based Access Control

**Channel Capabilities**: Each channel access requires a 64-bit cryptographic token
- **Generation**: Hardware entropy + TSC + per-core counters
- **Validation**: Constant-time lookup in kernel hash table
- **Delegation**: Capabilities can be passed via process creation or IPC

**Attack Resistance**:
- **Bruteforce Protection**: Invalid capability usage triggers escalating delays
- **Token Forgery**: Cryptographically infeasible to guess valid tokens
- **Privilege Escalation**: No ambient authority - all access requires explicit capabilities

### Information Flow Control

**Memory Isolation**: IPC buffers provide controlled information transfer
- **Send Isolation**: Sender cannot access buffer during receiver processing
- **Receive Isolation**: Receiver gets temporary controlled access to sender data
- **Cleanup Guarantee**: Kernel automatically unmaps buffers after reply

**Channel Isolation**: No shared state between different channels
- **Message Queues**: Per-channel queues prevent cross-contamination
- **Capability Tokens**: Unique per channel, cannot be reused

### Denial of Service Protection

**Resource Limits**:
- **Message Queue Depth**: Limited to prevent memory exhaustion
- **Buffer Size Limits**: Maximum 4KB per message
- **Channel Limits**: Per-process channel creation limits

**Fairness Mechanisms**:
- **Round-robin Scheduling**: Fair access to blocked receivers
- **Priority Inheritance**: High-priority senders don't get starved
- **Timeout Mechanisms**: Blocked operations can be interrupted

## Debugging and Monitoring

### IPC Debugging Features

**Debug Syscalls**: Special capabilities for IPC monitoring
```c
// Debug-only syscalls (require DEBUG_IPC capability)
anos_debug_list_channels();        // List all active channels
anos_debug_show_channel_stats();   // Show per-channel statistics  
anos_debug_trace_ipc_messages();   // Enable message tracing
```

**Kernel Debug Output**: Extensive debug logging available via compile-time flags
- `DEBUG_CHANNEL_IPC`: Log all IPC operations
- `DEBUG_IPC_VERBOSE`: Detailed message flow tracing
- `DEBUG_CAPABILITY_USAGE`: Track capability usage patterns

### Performance Monitoring

**IPC Statistics**: Per-channel performance metrics
- Message send/receive counts
- Average latency measurements
- Buffer utilization statistics
- Error rate tracking

**System-wide Metrics**: Global IPC health monitoring
- Total active channels
- Peak message queue depths
- Memory usage for IPC subsystem
- Capability token utilization

This IPC system provides a secure, efficient, and flexible foundation for all userspace communication in Anos, enabling complex service interactions while maintaining strict security boundaries and predictable performance characteristics.