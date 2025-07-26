# SYSTEM Services

> [!WARNING]
> This documentation is still **under construction** and **will** change as the system evolves.
>
> Some information in this document may represent future plans or ideas that may or may not be
> implemented. Direct code references represent a snapshot in time.
>
> It should not be taken and correct, complete, comprehensive or concrete until this warning is
> removed.

This document provides comprehensive technical documentation of the SYSTEM component - Anos's user-mode supervisor that serves as the bridge between the microkernel and higher-level OS services.

## Overview

SYSTEM is the most privileged userspace process in Anos, serving as the **user-mode supervisor** responsible for essential operating system services that are too complex for the minimal microkernel but too fundamental to leave to regular applications.

### Role in the Architecture

```
┌─────────────────────────────────────────────────────┐
│                KERNEL                               │
│ Memory Mgmt │ Scheduler │ IPC │ Timers & Interrupts │
└─────────────┬───────────────────────────────────────┘
              │ Comprehensive Capabilities
    ┌─────────▼───────────┐
    │      SYSTEM         │ ◄─── User-mode Supervisor
    │ ┌─────┬─────┬─────┐ │
    │ │ VFS │PROC │RAMFS│ │ ◄─── Core Services  
    │ └─────┴─────┴─────┘ │
    └─────────┬───────────┘
              │ Delegated Capabilities
    ┌─────────▼─────────┐
    │   Other Servers   │ ◄─── DEVMAN, Drivers, Apps
    └───────────────────┘
```

**Key Responsibilities**:
- **Process Creation**: ELF loading and process lifecycle management
- **Virtual File System**: Filesystem abstraction and driver coordination
- **Capability Delegation**: Security token management and distribution
- **System Bootstrap**: Initial server startup and service coordination

## Service Architecture

### Multi-Threaded Design

SYSTEM implements a **multithreaded service architecture** with dedicated threads for each major service:

```c
// system/main.c - Service thread allocation
anos_create_thread(ramfs_driver_thread, (uintptr_t)ramfs_driver_thread_stack);        // 256KiB stack
anos_create_thread(process_manager_thread, (uintptr_t)process_manager_thread_stack);  // 256KiB stack

// Main thread continues as VFS service thread
while (true) {
    // ... process VFS messages
}
```

**Thread Isolation**:
- Each service thread has **dedicated IPC buffer addresses**
- **Independent message queues** prevent service interference
- **Separate stack spaces** for isolation and debugging
- **Per-thread error handling** maintains service availability

### Service Registration

SYSTEM registers **two public named channels** for external access:

```c
// system/main.c - Named service registration
anos_register_channel_name(vfs_channel, "SYSTEM::VFS");
anos_register_channel_name(process_manager_channel, "SYSTEM::PROCESS");
```

**Service Discovery**:
- **`SYSTEM::VFS`**: File system driver lookup and routing
- **`SYSTEM::PROCESS`**: Process creation and management
- **RAMFS Channel**: Internal unnamed channel (delegated to clients)

## VFS Service (SYSTEM::VFS)

### Purpose and Design

The Virtual File System service provides **filesystem abstraction and driver routing** for all file operations in Anos.

**Architecture**:
- **Mount-point based routing**: Maps path prefixes to filesystem drivers
- **Driver delegation**: Returns filesystem driver channels to clients
- **Stateless operation**: No persistent file handles or caching

### Protocol Specification

**Channel**: `SYSTEM::VFS`  
**IPC Buffer**: `0xa0000000` (fixed address)  

#### VFS Operations

| Operation                  | Tag | Request Format | Response        | Description                         |
|----------------------------|-----|----------------|-----------------|-------------------------------------|
| **Find Filesystem Driver** | `1` | Path string    | Channel ID or 0 | Maps mount prefix to driver channel |

#### VFS_FIND_FS_DRIVER Operation

**Request**:
```c
#define VFS_FIND_FS_DRIVER (1)

// Request data: null-terminated path string
char path[] = "boot:/path/to/file";
uint64_t fs_channel = anos_send_message(vfs_channel, VFS_FIND_FS_DRIVER,
                                        strlen(path) + 1, path);
```

**Response**:
- **Success**: Filesystem driver channel ID (positive value)
- **Not Found**: `0` (mount point not recognized)
- **Error**: Negative error code

**Implementation** ([`system/main.c`](../system/main.c)):
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
            
            // Check for boot filesystem mount
            if (strncmp(path, "boot:", 5) == 0) {
                result = ramfs_channel;
            }
            // Future mount points would be checked here
            break;
            
        default:
            printf("VFS: Unknown operation %lu\n", tag);
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

### Mount Point Management

**Current Mount Points**:
- **`boot:`** - Maps to embedded RAMFS containing system binaries

**Mount Point Resolution**:
1. **Prefix Extraction**: Extract mount prefix from path (e.g., `"boot:"` from `"boot:/file"`)
2. **Driver Lookup**: Find registered filesystem driver for mount point
3. **Channel Delegation**: Return driver channel to client for direct access

**Future Extensions**:
```c
// Planned mount point support
if (strncmp(path, "dev:", 4) == 0) {
    result = devfs_channel;      // Device filesystem
} else if (strncmp(path, "tmp:", 4) == 0) {
    result = tmpfs_channel;      // Temporary filesystem
} else if (strncmp(path, "root:", 5) == 0) {
    result = ext4_channel;       // Root filesystem (ext4)
}
```

### VFS Client Usage Pattern

**Standard File Access Flow**:
```c
// Step 1: Discover VFS service
uint64_t vfs_channel = anos_find_named_channel("SYSTEM::VFS");

// Step 2: Get filesystem driver for path
char filepath[] = "boot:/program.elf";
uint64_t fs_channel = anos_send_message(vfs_channel, VFS_FIND_FS_DRIVER,
                                        strlen(filepath) + 1, filepath);

// Step 3: Use filesystem driver directly
if (fs_channel > 0) {
    // Query file size
    char filename[] = "/program.elf";
    uint64_t file_size = anos_send_message(fs_channel, FS_QUERY_OBJECT_SIZE,
                                          strlen(filename) + 1, filename);
    
    // Load file data
    uint64_t bytes_read = anos_send_message(fs_channel, FS_LOAD_OBJECT_PAGE,
                                           query_size, &load_query);
}
```

## Process Management Service (SYSTEM::PROCESS)

### Purpose and Design

The Process Management service provides **comprehensive process creation and lifecycle management** for all userspace processes.

**Architecture**:
- **ELF loading**: Complete ELF binary parsing and memory layout
- **Capability delegation**: Security token distribution to child processes
- **Trampoline loading**: Sophisticated bootstrap mechanism for new processes
- **Stack management**: Automatic stack layout and initialization

### Protocol Specification

**Channel**: `SYSTEM::PROCESS`  
**IPC Buffer**: `0xc0000000` (fixed address)  
**Thread**: Dedicated process manager thread (`system/main.c`)

#### Process Management Operations

| Operation | Tag | Request Format | Response | Description |
|-----------|-----|----------------|----------|-------------|
| **Process Spawn** | `1` | ProcessSpawnRequest | Process ID or error | Create new process with capabilities |

#### PROCESS_SPAWN Operation

**Request Structure**:
```c
#define PROCESS_SPAWN (1)

typedef struct {
    uint64_t stack_size;        // Stack size for new process (bytes)
    uint16_t argc;              // Command line argument count
    uint16_t capc;              // Capability count to delegate
    char data[];                // Packed data:
                               //   InitCapability capabilities[capc]
                               //   char *argv[argc] (null-terminated strings)
} ProcessSpawnRequest;

typedef struct {
    uint64_t capability_id;     // Syscall constant (e.g., SYSCALL_ID_DEBUG_PRINT)
    uint64_t capability_cookie; // Actual capability token from parent
} InitCapability;
```

**Response**:
- **Success**: Process ID (positive integer)
- **Error**: Negative error code
  - `-1`: Invalid parameters (argc > 64 or capc > 32)
  - `-2`: ELF loading failure
  - `-3`: Memory allocation failure
  - `-4`: Process creation failure

**Implementation** ([`system/main.c`](../system/main.c)):
```c
static void *process_manager_thread(void *arg) {
    while (1) {
        uint64_t tag, size;
        uint64_t message_cookie = anos_recv_message(process_manager_channel, &tag, &size,
                                                   (void*)0xc0000000);
        
        int64_t result = -1;
        switch (tag) {
        case PROCESS_SPAWN:
            ProcessSpawnRequest *request = (ProcessSpawnRequest*)0xc0000000;
            
            // Validate request parameters  
            if (request->argc <= 64 && request->capc <= 32) {
                // Extract packed capabilities and argv
                InitCapability *capabilities = (InitCapability*)request->data;
                char **argv = build_argv_from_packed_data(request);
                
                // Create new process
                result = create_server_process(request->stack_size, request->capc,
                                             capabilities, request->argc, argv);
                
                free(argv); // Clean up temporary argv array
            }
            break;
            
        default:
            printf("PROCESS: Unknown operation %lu\n", tag);
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

### Process Creation Mechanism

The process creation mechanism is one of the most sophisticated parts of SYSTEM, involving multiple phases:

#### Phase 1: Request Processing and Validation

```c
// system/process.c:create_server_process() - Entry point
int64_t create_server_process(uint64_t stack_size, uint16_t capc,
                             const InitCapability *capabilities,
                             uint16_t argc, const char **argv) {
    
    // Validate arguments
    if (argc == 0 || argv[0] == NULL) {
        return -1; // Invalid executable path
    }
    
    // Continue with process creation...
}
```

#### Phase 2: Memory Layout Preparation

```c
// system/process.c - Map SYSTEM code into new process
const SyscallResult map_result = anos_map_virtual(
    (void*)0x01000000,              // Target address in new process
    (void*)0x01000000,              // SYSTEM's code address  
    SYSTEM_MAPPED_SIZE,             // Size to map
    process_id,                     // Target process ID
    ANOS_MAP_VIRTUAL_FLAG_READ | ANOS_MAP_VIRTUAL_FLAG_EXEC
);
```

**Memory Layout for New Process**:
- **Code Region**: `0x01000000` - temporarily contains SYSTEM code
- **Stack Region**: `0x44000000` - stack top, grows downward
- **IPC Buffers**: Various addresses depending on service type

#### Phase 3: Stack Initialization

The stack initialization process (`system/process.c:build_new_process_init_values()`) is extremely sophisticated:

```c
// Initial stack layout (from high to low addresses):
// ┌─────────────────────┐ ← 0x44000000 (stack top)
// │   String Data       │   argv strings + paths
// ├─────────────────────┤
// │   Capabilities      │   InitCapability array[capc]  
// ├─────────────────────┤
// │   argv Pointers     │   char *argv[argc]
// ├─────────────────────┤
// │   capc, capv        │   Capability count & pointer
// ├─────────────────────┤  
// │   argc, argv        │   Argument count & pointer
// └─────────────────────┘ ← Initial stack pointer
```

**Stack Building Process**:
1. **Calculate Space**: Determine total space needed for all data
2. **String Placement**: Copy argv strings to top of stack
3. **Capability Array**: Place InitCapability structures
4. **Pointer Arrays**: Build argv pointer array with corrected addresses
5. **Final Layout**: Place argc/argv and capc/capv at stack bottom

#### Phase 4: Process Creation and Trampoline

```c
// system/process.c - Create process with trampoline entry
const int64_t process_id = anos_create_process(
    (void*)initial_server_loader,    // Trampoline entry point  
    stack_pointer,                   // Prepared stack
    stack_size                       // Stack size
);
```

**Trampoline Mechanism**:
1. **Assembly Trampoline** (`system/arch/x86_64/trampolines.asm`): Preserves stack and calls C function
2. **C Loader Function** (`system/loader.c:initial_server_loader_bounce()`): Performs ELF loading
3. **ELF Processing**: Loads actual target binary over SYSTEM code
4. **Memory Cleanup**: Unmaps SYSTEM code from new process
5. **Control Transfer**: Jumps to target binary entry point

### ELF Loading Process

The ELF loading subsystem (`system/elf.c` and `system/loader.c`) provides complete ELF64 binary loading:

#### ELF Validation and Parsing

```c
// system/elf.c:elf_map_elf64() - ELF header validation
bool elf_map_elf64(uint64_t channel, const char *filename, 
                   bool (*on_program_header)(const Elf64_Phdr*, uint64_t, void*),
                   void *callback_arg) {
    
    // Read and validate ELF header
    Elf64_Ehdr elf_header;
    load_file_data(channel, filename, 0, sizeof(elf_header), &elf_header);
    
    // Check ELF magic number
    if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) {
        return false; // Not a valid ELF file
    }
    
    // Validate 64-bit format
    if (elf_header.e_ident[EI_CLASS] != ELFCLASS64) {
        return false; // Not 64-bit ELF
    }
    
    // Process program headers
    for (int i = 0; i < elf_header.e_phnum; i++) {
        Elf64_Phdr phdr;
        load_program_header(channel, filename, &elf_header, i, &phdr);
        
        if (phdr.p_type == PT_LOAD) {
            if (!on_program_header(&phdr, channel, callback_arg)) {
                return false; // Callback reported error
            }
        }
    }
    
    return true;
}
```

#### Segment Loading

```c
// system/loader.c:on_program_header() - Load ELF segments
static bool on_program_header(const Elf64_Phdr *phdr, uint64_t channel, void *arg) {
    // Validate segment alignment
    if (phdr->p_vaddr & PAGE_RELATIVE_MASK) {
        printf("ERROR: Segment not page-aligned: 0x%lx\n", phdr->p_vaddr);
        return false;
    }
    
    // Calculate permission flags
    uint64_t map_flags = ANOS_MAP_VIRTUAL_FLAG_READ;
    if (phdr->p_flags & PF_W) map_flags |= ANOS_MAP_VIRTUAL_FLAG_WRITE;
    if (phdr->p_flags & PF_X) map_flags |= ANOS_MAP_VIRTUAL_FLAG_EXEC;
    
    // Map virtual memory for segment
    size_t segment_pages = (phdr->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
    SyscallResult map_result = anos_map_virtual((void*)phdr->p_vaddr, NULL,
                                               segment_pages * PAGE_SIZE, 0,
                                               map_flags);
    
    // Zero-fill entire segment
    memset((void*)phdr->p_vaddr, 0, phdr->p_memsz);
    
    // Load file data page by page
    const char *filename = (const char*)arg;
    for (size_t offset = 0; offset < phdr->p_filesz; offset += PAGE_SIZE) {
        size_t load_size = min(PAGE_SIZE, phdr->p_filesz - offset);
        load_file_page(channel, filename, phdr->p_offset + offset,
                      (void*)(phdr->p_vaddr + offset), load_size);
    }
    
    return true;
}
```

### Client Usage Examples

#### Basic Process Spawning

```c
// devman/main.c - DEVMAN spawning PCI driver
uint64_t process_channel = anos_find_named_channel("SYSTEM::PROCESS");

// Define capabilities to delegate
const InitCapability pci_caps[] = {
    {.capability_id = SYSCALL_ID_DEBUG_PRINT,
     .capability_cookie = __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
    {.capability_id = SYSCALL_ID_MAP_PHYSICAL,
     .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
    {.capability_id = SYSCALL_ID_MAP_VIRTUAL,
     .capability_cookie = __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
    // ... additional capabilities
};

// Define command line arguments
const char *pci_argv[] = {
    "boot:/pcidrv.elf",
    "0x80000000",           // ECAM base address
    "0",                    // PCI segment
    "0",                    // Start bus
    "255"                   // End bus
};

// Build packed request
ProcessSpawnRequest *request = build_spawn_request(
    0x100000,               // 1MB stack
    5,                      // argc
    sizeof(pci_caps)/sizeof(pci_caps[0]), // capc
    pci_caps,
    pci_argv
);

// Send spawn request
int64_t pid = anos_send_message(process_channel, PROCESS_SPAWN,
                               request_total_size, request);

if (pid > 0) {
    printf("PCI driver spawned with PID %ld\n", pid);
} else {
    printf("Process spawn failed: %ld\n", pid);
}
```

#### Advanced Process Spawning with Custom Capabilities

```c
// Custom capability set for specialized server
const InitCapability custom_caps[] = {
    // Basic operations
    {SYSCALL_ID_DEBUG_PRINT, __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
    {SYSCALL_ID_KILL_CURRENT_TASK, __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]},
    
    // Memory management
    {SYSCALL_ID_MAP_VIRTUAL, __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
    {SYSCALL_ID_UNMAP_VIRTUAL, __syscall_capabilities[SYSCALL_ID_UNMAP_VIRTUAL]},
    
    // IPC for service communication
    {SYSCALL_ID_CREATE_CHANNEL, __syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL]},
    {SYSCALL_ID_SEND_MESSAGE, __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]},
    {SYSCALL_ID_RECV_MESSAGE, __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE]},
    {SYSCALL_ID_REPLY_MESSAGE, __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE]},
    
    // Named channel access
    {SYSCALL_ID_FIND_NAMED_CHANNEL, __syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL]},
    
    // Hardware access (if needed)
    {SYSCALL_ID_MAP_PHYSICAL, __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
};

// Spawn specialized server with large stack
int64_t server_pid = spawn_process_with_capabilities(
    "boot:/specialized_server.elf",
    0x200000,               // 2MB stack for complex operations
    custom_caps,
    sizeof(custom_caps)/sizeof(custom_caps[0]),
    server_specific_args
);
```

## RAMFS Driver Service

### Purpose and Design

The RAMFS driver provides **file system operations** on the embedded boot filesystem that contains all system binaries and initial data.

**Architecture**:
- **Embedded filesystem**: RAMFS data embedded in kernel image during build
- **Read-only operations**: No write/modification support
- **Zero-copy delivery**: File data returned via buffer mapping
- **Linear search**: Simple file lookup via sequential scan

### Protocol Specification

**Channel**: Internal unnamed channel (delegated via VFS)  
**IPC Buffer**: `0xb0000000` (fixed address)  
**Thread**: Dedicated RAMFS thread ([`system/main.c`](../system/main.c))

#### RAMFS Operations

| Operation             | Tag | Request Format    | Response           | Description                       |
|-----------------------|-----|-------------------|--------------------|-----------------------------------|
| **Query Object Size** | `1` | Filename string   | File size in bytes | Get size of file                  |
| **Load Object Page**  | `2` | FileLoadPageQuery | Data bytes loaded  | Load file data starting at offset |

#### FS_QUERY_OBJECT_SIZE Operation

**Request**:
```c
#define FS_QUERY_OBJECT_SIZE (1)

// Request data: null-terminated filename (relative to filesystem root)
char filename[] = "/devman.elf";
uint64_t file_size = anos_send_message(ramfs_channel, FS_QUERY_OBJECT_SIZE,
                                      strlen(filename) + 1, filename);
```

**Response**:
- **Success**: File size in bytes (positive value)
- **Not Found**: `0` (file does not exist)

#### FS_LOAD_OBJECT_PAGE Operation

**Request**:
```c
#define FS_LOAD_OBJECT_PAGE (2)

typedef struct {
    uint64_t start_byte_ofs;    // Starting byte offset in file
    char name[];                // Filename (null-terminated)
} FileLoadPageQuery;

// Example usage
static char __attribute__((aligned(4096))) file_buffer[4096];
FileLoadPageQuery query = {
    .start_byte_ofs = 1024,     // Start loading from byte 1024
    // name will be copied after the struct
};
strcpy(query.name, "/program.elf");

uint64_t bytes_read = anos_send_message(ramfs_channel, FS_LOAD_OBJECT_PAGE,
                                       sizeof(query) + strlen(query.name) + 1,
                                       &query);
// file_buffer now contains up to 4096 bytes of file data
```

**Response**:
- **Success**: Number of bytes loaded into buffer (0 to 4096)
- **End of File**: `0` if start_byte_ofs ≥ file size
- **Not Found**: `0` if file does not exist

### RAMFS Data Structures

#### Filesystem Header

```c
// system/include/ramfs.h - RAMFS header format
typedef struct {
    uint32_t magic;         // 0x41524653 ("ARFS" - Anos RAMFS)
    uint32_t version;       // Format version (currently 1)
    uint32_t size;          // Total filesystem size in bytes
    uint32_t file_count;    // Number of files in filesystem
    uint32_t reserved[4];   // Reserved for future use
} AnosRAMFSHeader;
```

#### File Header Format

```c
// system/include/ramfs.h - File entry format  
typedef struct {
    uint32_t offset;        // Offset from filesystem start (bytes)
    uint32_t length;        // File size in bytes
    char name[16];          // Filename (null-terminated, max 15 chars)  
    uint32_t reserved[2];   // Reserved for future use (permissions, etc.)
} AnosRAMFSFileHeader;
```

### RAMFS Implementation

#### File Lookup

```c
// system/ramfs.c:ramfs_find_file() - Linear file search
const AnosRAMFSFileHeader *ramfs_find_file(const char *name) {
    extern char _binary_servers_system_ramfs_start[];
    const AnosRAMFSHeader *header = (const AnosRAMFSHeader*)_binary_servers_system_ramfs_start;
    
    // Validate filesystem magic
    if (header->magic != 0x41524653) {
        return NULL; // Invalid or corrupted filesystem
    }
    
    // Search through file headers
    const AnosRAMFSFileHeader *file_headers = (const AnosRAMFSFileHeader*)(header + 1);
    for (uint32_t i = 0; i < header->file_count; i++) {
        if (strcmp(file_headers[i].name, name) == 0) {
            return &file_headers[i]; // Found file
        }
    }
    
    return NULL; // File not found
}
```

#### File Data Access

```c  
// system/ramfs.c:ramfs_file_open() - Get pointer to file data
void *ramfs_file_open(const AnosRAMFSFileHeader *file) {
    extern char _binary_servers_system_ramfs_start[];
    const AnosRAMFSHeader *header = (const AnosRAMFSHeader*)_binary_servers_system_ramfs_start;
    
    // Calculate absolute file data address
    return (void*)((char*)header + file->offset);
}
```

#### Service Implementation

```c
// system/main.c - RAMFS service thread
static void *ramfs_driver_thread(void *arg) {
    while (1) {
        uint64_t tag, size;
        uint64_t message_cookie = anos_recv_message(ramfs_channel, &tag, &size,
                                                   (void*)0xb0000000);
        
        uint64_t result = 0;
        switch (tag) {
        case FS_QUERY_OBJECT_SIZE: {
            char *filename = (char*)0xb0000000;
            const AnosRAMFSFileHeader *file = ramfs_find_file(filename);
            result = file ? file->length : 0;
            break;
        }
        
        case FS_LOAD_OBJECT_PAGE: {
            FileLoadPageQuery *query = (FileLoadPageQuery*)0xb0000000;
            const AnosRAMFSFileHeader *file = ramfs_find_file(query->name);
            
            if (file && query->start_byte_ofs < file->length) {
                void *file_data = ramfs_file_open(file);
                size_t bytes_to_copy = min(4096, file->length - query->start_byte_ofs);
                
                // Copy file data to IPC buffer (zero-copy alternative possible)
                memcpy((void*)0xb0000000, 
                      (char*)file_data + query->start_byte_ofs,
                      bytes_to_copy);
                
                result = bytes_to_copy;
            }
            break;
        }
        
        default:
            printf("RAMFS: Unknown operation %lu\n", tag);
        }
        
        anos_reply_message(message_cookie, result);
    }
}
```

### RAMFS Build Integration

The RAMFS is created during the build process and embedded directly into the system binary:

```makefile
# servers/Makefile - RAMFS creation
system.ramfs: $(MKRAMFS_BIN) $(SERVER_BINARIES)
	$(MKRAMFS_BIN) system.ramfs $(SERVER_BINARIES)

# system/Makefile - RAMFS linking
system_ramfs_linkable.o: ../servers/system.ramfs
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64					\
				--rename-section .data=.ramfs							\
				../servers/system.ramfs system_ramfs_linkable.o
```

**Embedded Symbols**:
- `_binary_servers_system_ramfs_start` - Start of filesystem data
- `_binary_servers_system_ramfs_end` - End of filesystem data  
- `_binary_servers_system_ramfs_size` - Size of filesystem

## Capability Management

### Capability Flow Architecture

SYSTEM serves as the **primary capability distributor** in the Anos security model:

```
Kernel (Full Hardware Access)
    ↓ Grants comprehensive capabilities to SYSTEM
SYSTEM (Capability Authority)
    ├─ VFS Service (Read-only capabilities)
    ├─ Process Service (Process creation capabilities)  
    └─ Child Processes (Delegated subset capabilities)
```

### Capability Reception

SYSTEM receives all kernel syscall capabilities during initialization:

```c
// system/main.c - SYSTEM capability acquisition
extern uint64_t __syscall_capabilities[];

// Capabilities acquired by SYSTEM:
__syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]        // Debug output
__syscall_capabilities[SYSCALL_ID_DEBUG_CHAR]         // Character output
__syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]        // Virtual memory mapping
__syscall_capabilities[SYSCALL_ID_UNMAP_VIRTUAL]      // Virtual memory unmapping
__syscall_capabilities[SYSCALL_ID_ALLOC_PHYSICAL_PAGES] // Physical page allocation
__syscall_capabilities[SYSCALL_ID_CREATE_PROCESS]     // Process creation
__syscall_capabilities[SYSCALL_ID_CREATE_THREAD]      // Thread creation
__syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]  // Task termination
__syscall_capabilities[SYSCALL_ID_TASK_SLEEP_CURRENT] // Sleep operations
__syscall_capabilities[SYSCALL_ID_CREATE_CHANNEL]     // IPC channel creation
__syscall_capabilities[SYSCALL_ID_DESTROY_CHANNEL]    // IPC channel destruction
__syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]       // IPC message sending
__syscall_capabilities[SYSCALL_ID_RECV_MESSAGE]       // IPC message receiving
__syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE]      // IPC message replies
__syscall_capabilities[SYSCALL_ID_REGISTER_CHANNEL_NAME] // Named channel registration
__syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL] // Named channel discovery
__syscall_capabilities[SYSCALL_ID_MAP_FIRMWARE_TABLES] // ACPI table access
__syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]       // Physical memory mapping
__syscall_capabilities[SYSCALL_ID_GET_MEM_INFO]       // Memory information
```

### Capability Delegation Patterns

#### Hardware Access Delegation

```c
// devman/main.c - DEVMAN capability set
const InitCapability devman_caps[] = {
    // Debug and basic operations
    {SYSCALL_ID_DEBUG_PRINT, __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
    {SYSCALL_ID_DEBUG_CHAR, __syscall_capabilities[SYSCALL_ID_DEBUG_CHAR]},
    {SYSCALL_ID_KILL_CURRENT_TASK, __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]},
    
    // Hardware access for device discovery
    {SYSCALL_ID_MAP_FIRMWARE_TABLES, __syscall_capabilities[SYSCALL_ID_MAP_FIRMWARE_TABLES]},
    {SYSCALL_ID_MAP_PHYSICAL, __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
    {SYSCALL_ID_MAP_VIRTUAL, __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
    
    // Process creation for driver spawning
    {SYSCALL_ID_CREATE_PROCESS, __syscall_capabilities[SYSCALL_ID_CREATE_PROCESS]},
    
    // IPC for service communication
    {SYSCALL_ID_SEND_MESSAGE, __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]},
    {SYSCALL_ID_FIND_NAMED_CHANNEL, __syscall_capabilities[SYSCALL_ID_FIND_NAMED_CHANNEL]},
    
    // Memory management
    {SYSCALL_ID_ALLOC_PHYSICAL_PAGES, __syscall_capabilities[SYSCALL_ID_ALLOC_PHYSICAL_PAGES]},
};
```

#### Driver Capability Sets

```c
// Driver capabilities (more restricted)
const InitCapability driver_caps[] = {
    // Basic operations only
    {SYSCALL_ID_DEBUG_PRINT, __syscall_capabilities[SYSCALL_ID_DEBUG_PRINT]},
    {SYSCALL_ID_KILL_CURRENT_TASK, __syscall_capabilities[SYSCALL_ID_KILL_CURRENT_TASK]},
    
    // Hardware access for specific device
    {SYSCALL_ID_MAP_PHYSICAL, __syscall_capabilities[SYSCALL_ID_MAP_PHYSICAL]},
    {SYSCALL_ID_MAP_VIRTUAL, __syscall_capabilities[SYSCALL_ID_MAP_VIRTUAL]},
    
    // Limited IPC (no process creation)
    {SYSCALL_ID_SEND_MESSAGE, __syscall_capabilities[SYSCALL_ID_SEND_MESSAGE]},
    {SYSCALL_ID_RECV_MESSAGE, __syscall_capabilities[SYSCALL_ID_RECV_MESSAGE]},
    {SYSCALL_ID_REPLY_MESSAGE, __syscall_capabilities[SYSCALL_ID_REPLY_MESSAGE]},
};
```

### Security Properties

**Principle of Least Privilege**: Each process receives only the minimum capabilities required for its function.

**Capability Attenuation**: SYSTEM can delegate a subset of its capabilities but cannot grant capabilities it doesn't have.

**No Capability Amplification**: Child processes cannot gain additional capabilities beyond what their parent delegates.

**Revocation**: Process termination automatically revokes all delegated capabilities (handled by kernel).

## Performance Characteristics

> [!WARNING]
> Numbers in this section are aspirational, and do not represent tested values on
> any platform.
>
> They should not be taken and correct, complete, comprehensive or concrete until
> this warning is removed.

### Service Thread Performance

**IPC Latency**: ~1-2 microseconds per service request (modern hardware)
- **VFS lookups**: ~500ns (simple string comparison)  
- **Process creation**: ~50-100ms (ELF loading + memory setup)
- **RAMFS operations**: ~1-5 microseconds (depending on file size)

**Throughput**: 
- **VFS**: ~500,000 lookups/second per thread
- **RAMFS**: ~200,000 file operations/second per thread  
- **Process creation**: ~10-20 processes/second (limited by ELF loading)

### Memory Usage

**SYSTEM Process**: ~2-4MB total memory usage
- **Code segment**: ~500KB (ELF + libraries)
- **Stack space**: ~1.5MB (main + service threads)  
- **Heap usage**: ~512KB (dynamic allocations)
- **IPC buffers**: 12KB (3 fixed buffers @ 4KB each)

**Per-Service Overhead**: ~256KB per service thread (stack space)

### Scalability Characteristics

**Multi-threading**: Each service runs independently with dedicated resources
**Load balancing**: Not implemented (single thread per service type)
**Bottlenecks**: ELF loading during process creation, linear RAMFS file search

## Error Handling and Diagnostics

### Error Reporting

All SYSTEM services provide **comprehensive error reporting**:

```c
// Standard error codes used across services
#define ERROR_INVALID_PARAMETERS    (-1)
#define ERROR_FILE_NOT_FOUND       (-2)  
#define ERROR_MEMORY_ALLOCATION    (-3)
#define ERROR_PROCESS_CREATION     (-4)
#define ERROR_ELF_LOADING          (-5)
#define ERROR_CAPABILITY_DENIED    (-6)
```

### Debug Output

SYSTEM provides extensive debug output controlled by compile-time flags:

```c
// Debug output examples throughout SYSTEM code
printf("VFS: Looking up filesystem for path: %s\n", path);
printf("PROCESS: Creating process with %d capabilities\n", capc);
printf("RAMFS: File '%s' size: %lu bytes\n", filename, file_size);
printf("ELF: Loading segment at 0x%lx, size %lu\n", phdr->p_vaddr, phdr->p_memsz);
```

### Service Health Monitoring

**Service Availability**: Each service thread runs independently - failure of one service doesn't affect others.

**Resource Monitoring**: SYSTEM tracks memory usage and reports statistics via debug interfaces.

**Diagnostic Interfaces**: Special debug capabilities allow monitoring of SYSTEM internal state.

This comprehensive service architecture enables SYSTEM to serve as the reliable foundation for Anos's userspace ecosystem while maintaining strict security boundaries and providing the essential services required by all other system components.