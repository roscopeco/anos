## Syscalls for Anos

### High-level overview

The `STAGE3` kernel exposes a minimal set of low-level system calls
for user-mode software that requires access to kernel-provided services
such as memory management and execution control. 

Collectively, these functions comprise the `STAGE3` System Call 
(syscall) interface.

As a microkernel, the functionality provided via syscalls is intentionally
minimal and limited to core operations that cannot be safely or portably 
implemented in user space. These operations typically involve privileged 
access or system integrity concerns.

In most cases, user-mode applications will not invoke system calls directly.
Instead, they are expected to use higher-level wrappers provided by runtime 
libraries—such as libanos, available as part of the standard Anos toolchain.

Unlike traditional monolithic kernels, `STAGE3` does not implement 
functionality like file I/O or networking via system calls. Instead, such
services are provided by user-mode servers (e.g. `SYSTEM`) and accessed 
through Inter-Process Communication (IPC). This document only describes the
direct kernel syscall interface and does not cover IPC mechanisms or
user-mode service protocols.

### System Call Capabilities

In `STAGE3`, access to system calls is governed entirely by capabilities. 
There is no ambient authority, and no access checks are performed by the 
system calls themselves. Possession of the appropriate capability by a 
caller unconditionally confers the ability to invoke the corresponding 
system call, subject only to any restrictions encoded within the capability itself.

By default, programs in Anos operate with a minimal set of system call 
capabilities, limiting them to a narrow subset of system-level operations.

System call capabilities are always inherited from a parent process during process
creation. The kernel does not provide any mechanism for processes to request or
acquire additional capabilities at runtime.

A process that holds the capability to spawn new processes may delegate some
or all of its own system call capabilities to its children. 
Delegated capabilities may be passed through unchanged or refined to enforce stricter
controls. However, a process cannot delegate any capability it does not itself possess.

For a more detailed explanation of capabilities and their role within `STAGE3` and 
Anos as a whole, see the [Capabilities documentation](/docs/Capabilities.md).

### System Call Reference

Below is the list of direct kernel system calls provided by the `STAGE3` interface. 
All calls require possession of a corresponding capability token. 
All pointer arguments must refer to valid memory within the caller’s address space.
Syscall ID numbers not defined below are reserved.

> [!WARNING]
> These syscall IDs and the specifics of the functionality provided are
> subject to change, until such time as a major version of Anos is released.
> 
> Once Anos is major-versioned, each major-version will guarantee a stable
> (i.e. self-consistent) system call ID scheme and function interfaces.

---

#### Call ID 1: `SyscallResult anos_kprint(const char *msg)`

Writes a null-terminated string to the kernel debug output.

* **Parameters:**
  `msg` – Pointer to a null-terminated UTF-8 string.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 2: `SyscallResult anos_kputchar(char chr)`

Writes a single character to the kernel debug output.

* **Parameters:**
  `chr` – The character to output.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 3: `SyscallResult anos_create_thread(ThreadFunc func, uintptr_t stack_pointer)`

Creates a new thread in the calling process.

* **Parameters:**
  `func` – Pointer to the thread entry function.
  `stack_pointer` – Initial stack pointer for the new thread.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the thread ID (TID) on success.

---

#### Call ID 4: `SyscallResult anos_get_mem_info(AnosMemInfo *meminfo)`

Retrieves basic memory usage statistics for the calling process.

* **Parameters:**
  `meminfo` – Pointer to a `AnosMemInfo` structure to populate.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 5: `SyscallResult anos_task_sleep_current(uint64_t ticks)`

Suspends the current thread for a number of scheduler ticks.

* **Parameters:**
  `ticks` – Number of ticks to sleep.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 6: `SyscallResult anos_create_process(ProcessCreateParams *params)`

Creates a new process.

* **Parameters:**
  `params` – Pointer to a populated `ProcessCreateParams` structure.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the process ID (PID) on success.

---

#### Call ID 7: `SyscallResult anos_map_virtual(uint64_t size, uintptr_t base_address, uint64_t flags)`

Requests allocation and virtual memory mapping in the process's 
address space.

When successful, the memory returned will be zeroed and ready for
use according to the flags given.

> [!NOTE]
> This function _may_ not allocate physical memory immediately,
> and lazy allocation may be employed instead.

* **Parameters:**
  `size` – Size in bytes to map (must be page-aligned).
  `base_address` – Desired base virtual address (must be page-aligned).
  `flags` – Mapping flags (e.g., read/write/exec - see `anos/syscalls.h`).

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the address of the mapped region on success.

---

#### Call ID 8: `SyscallResult anos_send_message(uint64_t channel_cookie, uint64_t tag, size_t buffer_size, void *buffer)`

Sends a message to the specified IPC channel.

* **Parameters:**
  `channel_cookie` – Capability identifying the IPC channel.
  `tag` – User-defined message tag.
  `buffer_size` – Size of the message payload.
  `buffer` – Pointer to message buffer.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the message cookie on success.

---

#### Call ID 9: `SyscallResult anos_recv_message(uint64_t channel_cookie, uint64_t *tag, size_t *buffer_size, void *buffer)`

Receives a message from the specified IPC channel. Blocks until a message is available.

* **Parameters:**
  `channel_cookie` – Capability identifying the IPC channel.
  `tag` – Out: message tag.
  `buffer_size` – In: max buffer size; out: actual received size.
  `buffer` – Pointer to receive buffer.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the message cookie on success.

---

#### Call ID 10: `SyscallResult anos_reply_message(uint64_t message_cookie, uint64_t reply)`

Sends a reply to a received message.

* **Parameters:**
  `message_cookie` – Cookie returned by `recv_message`.
  `reply` – Reply value.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the message cookie on success.

---

#### Call ID 11: `SyscallResult anos_create_channel(void)`

Creates a new IPC channel.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the capability cookie for the new channel on success.

---

#### Call ID 12: `SyscallResult anos_destroy_channel(uint64_t cookie)`

Destroys an IPC channel.

* **Parameters:**
  `cookie` – Capability identifying the channel.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 13: `SyscallResult anos_register_channel_name(uint64_t cookie, char *name)`

Registers a human-readable name for an IPC channel.

* **Parameters:**
  `cookie` – Capability for the channel.
  `name` – Null-terminated UTF-8 name.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 14: `SyscallResult anos_remove_channel_name(char *name)`

Unregisters a channel name.

* **Parameters:**
  `name` – Null-terminated UTF-8 string.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 15: `SyscallResult anos_find_named_channel(char *name)`

Resolves a registered channel name to a capability.

* **Parameters:**
  `name` – Null-terminated UTF-8 name.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the channel capability cookie on success.

---

#### Call ID 16: `noreturn void anos_kill_current_task(void)`

Terminates the current thread.

* **Returns:**
  Does not return.

---

#### Call ID 17: `SyscallResult anos_unmap_virtual(uint64_t size, uintptr_t base_address)`

Unmaps a previously mapped virtual memory region.

* **Parameters:**
  `size` – Size in bytes (must match original mapping).
  `base_address` – Start address of the region.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 18: `SyscallResult anos_create_region(uintptr_t start, uintptr_t end, uint64_t flags)`

Declares a virtual memory region with specified access semantics.

* **Parameters:**
  `start` – Start address (inclusive).
  `end` – End address (exclusive).
  `flags` – Access flags.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 19: `SyscallResult anos_destroy_region(uintptr_t start)`

Destroys a previously created virtual region.

* **Parameters:**
  `start` – Start address of the region to destroy.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 20: `SyscallResult anos_map_firmware_tables(ACPI_RSDP *user_rsdp)`

Maps firmware tables (ACPI) from kernel space to user space and hands over control.

> [!NOTE]
> This syscall is only available on x86_64 architecture. On other architectures,
> it returns `SYSCALL_NOT_IMPL`.

* **Parameters:**
  `user_rsdp` – Pointer to user space buffer to receive the RSDP structure.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 21: `SyscallResult anos_map_physical(uintptr_t phys_addr, uintptr_t user_vaddr, size_t size, uint64_t flags)`

Maps physical memory pages into user virtual address space.

* **Parameters:**
  `phys_addr` – Physical address to map (must be page-aligned).
  `user_vaddr` – User virtual address to map to (must be page-aligned).
  `size` – Size in bytes to map (must be page-aligned).
  `flags` – Mapping flags (see `ANOS_MAP_PHYSICAL_FLAG_*` constants).

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

#### Call ID 22: `SyscallResult anos_alloc_physical_pages(size_t size)`

Allocates contiguous physical memory pages.

* **Parameters:**
  `size` – Size in bytes to allocate (must be page-aligned).

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the physical address of the allocated pages on success.

---

#### Call ID 23: `SyscallResult anos_alloc_interrupt_vector(uint32_t bus_device_func, uint64_t *msi_address, uint32_t *msi_data)`

Allocates an interrupt vector for MSI/MSI-X interrupts.

> [!NOTE]
> This syscall is only available on x86_64 architecture. On other architectures,
> it returns `SYSCALL_NOT_IMPL`.

* **Parameters:**
  `bus_device_func` – PCI bus/device/function identifier.
  `msi_address` – Pointer to receive MSI address for device configuration.
  `msi_data` – Pointer to receive MSI data for device configuration.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field containing the allocated interrupt vector number on success.

---

#### Call ID 24: `SyscallResult anos_wait_interrupt(uint8_t vector, uint32_t *event_data)`

Waits for an interrupt on the specified vector.

> [!NOTE]
> This syscall is only available on x86_64 architecture. On other architectures,
> it returns `SYSCALL_NOT_IMPL`.

* **Parameters:**
  `vector` – Interrupt vector number to wait for.
  `event_data` – Pointer to receive interrupt event data.

* **Returns:**
  `SyscallResult` struct with `type` field indicating success (`SYSCALL_OK`) or failure (negative error code), and `value` field set to `0`.

---

### Return Values

#### System Call Result Structure

System calls in Anos return a `SyscallResult` struct, which contains two 64-bit fields:
a `type` field indicating success or failure, and a `value` field for return data.

#### Structure: `SyscallResult`

> [!NOTE]
> This can be found in `anos/syscalls.h`


```c
typedef struct {
    SyscallResultType type;
    uint64_t value;
} SyscallResult;

typedef enum {
    SYSCALL_OK = 0ULL,
    SYSCALL_FAILURE = -1ULL,
    SYSCALL_BAD_NUMBER = -2ULL,
    SYSCALL_NOT_IMPL = -3ULL,
    SYSCALL_BADARGS = -4ULL,
    SYSCALL_BAD_NAME = -5ULL,

    /* ... reserved ... */
    SYSCALL_INCAPABLE = -254ULL
} SyscallResultType;
```

##### Meaning of Type Values

| Name                 | Value  | Meaning                                                               |
| -------------------- | ------ | --------------------------------------------------------------------- |
| `SYSCALL_OK`         | `0`    | The system call completed successfully.                               |
| `SYSCALL_FAILURE`    | `-1`   | Generic failure; reason is not specified.                             |
| `SYSCALL_BAD_NUMBER` | `-2`   | Invalid or unrecognized syscall number or capability.                 |
| `SYSCALL_NOT_IMPL`   | `-3`   | The requested system call exists but is not implemented.              |
| `SYSCALL_BADARGS`    | `-4`   | One or more arguments were invalid or out of bounds.                  |
| `SYSCALL_BAD_NAME`   | `-5`   | The specified name was not found or could not be resolved.            |
| `SYSCALL_INCAPABLE`  | `-254` | The caller lacks the required capability for the requested operation. |

#### Notes

* `SyscallResult` is a 16-byte struct containing two 64-bit fields.
* The `type` field uses `SyscallResultType` enum values and is consistent across architectures.
* A `type` value of `SYSCALL_OK` (`0`) universally indicates success.
* The `value` field contains syscall-specific return data (e.g., process IDs, addresses, cookies) when successful, or is typically `0` for simple success/failure calls.
* Additional error codes may be added in future; values `-6` through `-253` are currently unassigned.

## User-mode Interface Specification

### x86\_64 Syscall Interface

Anos provides two functionally equivalent mechanisms for invoking system calls on x86\_64:

* The `syscall` instruction (preferred)
* A software interrupt via `int 0x69` (fallback / compatibility)

The `syscall` instruction is faster and should be used in all performance-sensitive code.

#### Calling Convention

Both interfaces share a unified calling convention:

| Role             | Register |
| ---------------- | -------- |
| Capability token | `r9`     |
| Argument 1       | `rdi`    |
| Argument 2       | `rsi`    |
| Argument 3       | `rdx`    |
| Argument 4       | `r10`    |
| Argument 5       | `r8`     |
| Return value     | `rax`    |

Register preservation follows the **System V x86\_64 ABI**, with the following exceptions:

* `rcx` is *not* used for arguments and is clobbered by `syscall`
* `r10` is used in place of `rcx` (required by `syscall` encoding)

> There are no stack alignment constraints enforced by the kernel, but future versions may require standard 16-byte alignment.

#### Example Shim (syscall)

```asm
anos_testcall:
    mov     r9, 0x12345678      ; Syscall capability
    mov     r10, rcx            ; SysV 4th arg -> syscall 4th arg
    syscall
    ret
```

#### Example Shim (int 0x69)

```asm
anos_testcall:
    mov     r9, 0x12345678
    mov     r10, rcx
    int     0x69
    ret
```

---

### riscv64 Syscall Interface

System calls on `riscv64` are made using the `ecall` instruction, with arguments and 
return values passed through the standard integer argument registers.

#### Calling Convention

| Role             | Register |
| ---------------- | -------- |
| Capability token | `a7`     |
| Argument 1       | `a0`     |
| Argument 2       | `a1`     |
| Argument 3       | `a2`     |
| Argument 4       | `a3`     |
| Argument 5       | `a4`     |
| Return value     | `a0`     |

Register usage and calling conventions conform fully to the **standard RISC-V ABI**. 
Stack alignment and register preservation follow ABI rules and are not modified by 
the syscall dispatcher.

#### Example Shim

```asm
anos_testcall:
    li      a7, 0x12345678       # Syscall capability token
    ecall                        # Invoke syscall
    ret
```

## Kernel Interface Specification

### x86\_64

#### Dispatch

System calls are dispatched via two separate mechanisms:

* `syscall_enter` in `kernel/init_syscalls.asm` handles calls via the `syscall` instruction
* `syscall_69_handler` in `kernel/isr_dispatch.asm` handles calls via `int 0x69`

Both ultimately dispatch to the unified handler function `handle_syscall_69` defined
in `kernel/syscalls.c`. The dispatchers normalize argument passing between the two
mechanisms, ensuring a consistent call interface:

```c
SyscallResult handle_syscall_69(SyscallArg arg0, SyscallArg arg1,
                                SyscallArg arg2, SyscallArg arg3,
                                SyscallArg arg4, SyscallArg syscall_num);
```

Where `SyscallArg` and `SyscallResult` are both 64-bit signed integers. 
Interpretation of argument semantics is syscall-specific.

For example, a user-mode C call with prototype:

```c
int64_t anos_testcall(int64_t arg0, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4);
```

Will be dispatched internally as:

```c
return handle_syscall_69(arg0, arg1, arg2, arg3, arg4, 0);
```

Where `0` represents the syscall capability token (see calling convention for details).

#### Stack & Environment

System calls will eventually execute on the kernel stack associated with the calling 
thread, with `RSP` and segment selectors (`SS`) correctly configured. However, 
this is currently **TODO**.

At present, `syscall`-initiated calls set `SS` and `DS` to kernel values, but retain
the user-mode `RSP`. This is a temporary implementation state and subject to change.

---

### riscv64

> \[!WARNING]
> This section is still under construction.
>
> Refer to the x86\_64 dispatch description above for a general structure;
> the RISC-V syscall entry path follows similar principles in argument
> normalization and delegation to a central handler.
