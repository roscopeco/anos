## Syscalls for Anos

### User-mode Interface Spec

Anos provides two parallel interfaces for system calls:

* x86_64 `syscall` instruction (the "fast call interface")
* x86 software interrupt via `int 0x69` (the "slow call interface")

These are functionally equivalent, but the former is faster and so should
be preferred by user code wherever possible (which _should_ be basically
everywhere =])

#### Calling Convention

Both `syscall` and `int` interfaces have the same calling convention:

* Syscall number passed in `r9`
* Up to five register arguments in `rdi`, `rsi`, `rdx`, `r10` and `r8`
* Return value in `rax`
* Preserved registers: `rbx`, `rsp`, `rbp`, `r12`, `r13`, `r14`, and `r15`
* Scratch registers: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`, `r10`, `r11`
* No stack-alignment requirements (this may change in future)

This is _close_ to the SysV x86_64 ABI, but not _the same_ due to differences
required by the syscall interface (e.g. 4th argument in `r10` rather than
`rcx` due to `syscall` using the latter for the return address.)

A (simplified) example of a C-callable syscall shim for call #0 
(currently implemented as `TESTCALL`) might be:

```
testcall:
    xor r9, r9              ; Zero syscall number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    syscall                 ; Make the call
    ret
```

for the `syscall` interface, or:

```
testcall:
    xor r9, r9              ; Zero syscall number in r9
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls
    int 0x69                ; Make the call
    ret
```

for the `int 0x69` one. 

### Kernel Interface Spec

#### Dispatch

Syscalls are dispatched in two places, one for `syscall` instructions
(`syscall_enter` in `kernel/init_syscalls.asm`) and the other for
`int 0x69` software interrupts (`syscall_69_handler` in `kernel/isr_dispatch.asm`).
Both of these dispatch to the increasingly-inaptly-named `handle_syscall_69` 
in `kernel/syscalls.c`, with the dispatchers responsible for smoothing
out differences between the `syscall` and interrupt interfaces and
presenting a standard call sequence to the handler itself:

```C
SyscallResult handle_syscall_69(SyscallArg arg0, SyscallArg arg1,
                                SyscallArg arg2, SyscallArg arg3,
                                SyscallArg arg4, SyscallArg syscall_num);
```

where `SyscallArg` and `SyscallResult` are both 64-bit integer types (nominally 
signed, but actual intepretation is up to the individual syscall).

Using the examples in the `Syscall Argument Passing` section, above, if called 
from C with the prototype:

```C
int64_t testcall(int64_t arg0, int64_t arg1, int64_t arg2, int64_t arg3, int64_t arg4);
```

It would routed through the dispatch layer and wind up calling into 
`handle_syscall_69` with the original arguments, and the call number
(0 in this case) appended, i.e (conceptually):

```C
return handle_syscall_69(arg0, arg1, arg2, arg3, arg4, 0);
```

#### Stack & Environment

Syscalls _will_ run with the process' kernel stack in RSP (and the appropriate `SS` for
kernel) but this is still **TODO**! 

Currently, `syscall` calls run with the kernel `SS` and `DS`, but `rsp` still as set
at entry, which is weird, but there you go.
