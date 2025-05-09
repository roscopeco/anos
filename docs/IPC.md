## Interprocess Communication (IPC) Mechanisms

> [!WARNING]
> The IPC design is still evolving, so there's a good chance 
> some of the details here will change as development progresses.

Anos provides two fundamental primitives for IPC, available to
all processes (provided they are endowed by their supervisor with
appropriate capabilities) via the fast system call interface.

> [!WARNING]
> Shared memory regions are not currently exposed to userspace,
> this is work-in-progress.

* Synchronous message-passing (non-copying)
* Shared memory regions with dynamic read/write mapping

> [!NOTE]
> On x86_64, IPC is available via both the fast (`SYSCALL`) and 
> slow (`int 0x69`) system call interfaces.

Message-passing is intended to be the primary means of IPC in
Anos, and should be preferred for most use-cases.

Anos' message-passing provides full-featured, synchronous IPC
for both fast-path "ping" messaging (based soley on a single 64-bit
parameter) and larger payloads via automated interprocess buffer
mapping (i.e. zero-copy messaging). For both, the interfaces
remain the same and are described in this document.

The Anos libc implementation provides full support for an easy-to-use
wrapper interface for the relevant system calls. 

Processes wishing to use IPC features will of course require the
appropriate syscall capabilities, as well as the capability cookie
for the channel(s) they wish to communicate via.

### Usage - messaging

> [!WARNING]
> Features to mitigate priority inversion as a result of message
> passing are currently `TODO` in the calls within this section.
>
> Ultimately we'll implement a system similar to e.g. QNX, whereby
> senders and receivers will inherit priorities in a fashion 
> suitable to avoid inversion - but currently that will not 
> happen and inversion is not only possible, but almost certainly
> inevitable.

#### Message channels

All message-passing happens in the context of a message channel.
These are created and destroyed as needed by processes that wish
to provide service.

Message channels provide the following features:

* A locally-unique (system-scoped) capability identifier for the channel
* A message-queueing capability
* Automatic prioritised receiver queueing

##### `anos_create_channel` - Create a message channel

> C (or `extern C`) call-seq: `uint64_t anos_create_channel(void)`
>
> Arguments:
>   none
>
> Modifies (x86_64):
>   `rax` - channel cookie, or `0` on failure (**C Return Value**)
>   `r11` - trashed
>   `rcx` - trashed
>
> Blocks: Never

This call will create a new message channel, owned by the calling process,
and return a `uint64_t` capability cookie that must be used for all further
communication with the channel.

Should an error be encountered, this will return `0`.

Once a channel is created, it is ready for immediate use via its
cookie - which must be shared with other processes by a non-IPC
mechanism in order for them to make use of it.

Additionally, the kernel provides the ability for clients to
associate their channels with a "global name", which can then
be used by other processes to obtain the relevant capability cookie.

> [!NOTE]
> This global name feature is not likely to stick around - it's 
> a kind of ambient-authority that doesn't really fit well in our 
> capability model, and in any event there's no need for the kernel
> to be involved in it - even if it is useful, it could be provided
> by SYSTEM.
> 
> For this reason, the current scheme is not documented here, beyond
> the mention already made above.

##### `anos_destroy_channel` - Destroys a message channel

> C (or `extern C`) call-seq: `int anos_destroy_channel(uint64_t cookie)`
>
> * Arguments (x86_64):
>   * `rdi` - cookie
>
> * Modifies (x86_64):
>   * `rax` - `0` on success, or negative on failure (**C Return Value**)
>   * `r11` - trashed
>   * `rcx` - trashed
>
> Blocks: Never

This call destroys an existing message channel.

When destroying a channel, take note the following important points:

* Any blocked receivers will be unblocked with a message cookie of `0`
* Any senders of queued messages will be unblocked with a reply of `0`
* Any queued messages will be dropped

Where messages are already in-flight (i.e. have been returned from
a `anos_recv_message` call) processing will continue as normal - from
that point on communication relies directly on the message cookie 
rather than the (now invalid) channel cookie. Senders of such messages
will remain blocked until the handler calls `anos_reply_message` with
their message cookie.

#### Sending, Receiving and Replying

> [!NOTE]
> When sending and receiving messages, the `buffer` may be `NULL`, and
> the `buffer_size` may be zero - in this case, no address-space
> manipulation will occur.
> 
> This feature exists to support simple notification "pings" via a fast 
> path with only message tags.

##### `anos_send_message` - Send a message to a channel

> C (or `extern C`) call-seq: `uint64_t anos_send_message(uint64_t channel_cookie, uint64_t tag, size_t buffer_size, void *buffer);`
>
> * Arguments (x86_64):
>   * `rdi` - Destination channel cookie
>   * `rsi` - message tag
>   * `rdx` - buffer size
>   * `r10` - buffer pointer
>
> * Modifies (x86_64):
>   * `rax` - Reply (**C Return Value**)
>   * `r11` - trashed
>   * `rcx` - trashed
>
> Blocks: Unless error, blocks always, until reply received

Sends a message to a channel.

The caller will be blocked until the message is handled by some other
thread and `anos_reply_message` called. 

Once unblocked, this call will return the argument passed to `anos_reply_message`.

If the `buffer` parameter is provided (and `buffer_size` is non-zero) then
the kernel will arrange for the specified memory to be mapped into the 
receiving process where necessary at the time the message is received.

##### `anos_recv_message` - Receive a message from a channel

> C (or `extern C`) call-seq: `uint64_t anos_recv_message(uint64_t channel_cookie, uint64_t *tag, size_t *buffer_size, void *buffer);`
>
> * Arguments (x86_64):
>   * `rdi` - Source channel cookie
>   * `rsi` - message tag
>   * `rdx` - buffer size
>   * `r10` - buffer pointer
>
> * Modifies (x86_64):
>   * `rax` - Message cookie (**C Return Value**)
>   * `r11` - trashed
>   * `rcx` - trashed
>
> Blocks: If no messages are queued, will block until messages become available

Receives a message from a channel.

If messages are queued (and no higher-priority receiver is listening to the 
channel) this will return immediately with the next message to handle.

Otherwise, the caller will be blocked until there exists a message to be 
handled (sent from some other thread with `anos_send_message`).

> [!NOTE]
> The `tag` (reg: `rsi`) and `buffer_size` (reg: `rdx`) arguments must contain
> pointers to a memory locations that will receive the respective values as 
> passed to the `anos_send_message` call.
>
> The kernel will validate these pointers are valid userspace pointers, but will
> not check they are valid memory - if they are not, then the usual behaviour
> for invalid memory access will occur (by default, a page fault resulting in
> the process being killed).
> 
> The `buffer` argument itself should point to an **unallocated** virtual 
> address into which the sender's buffer will be mapped directly for the 
> duration of handling the call.

Once unblocked, this call will return the message cookie being handled, with 
values passed to `anos_send_message` populated into the given pointers.

##### `anos_reply_message` - Reply to a message 

> C (or `extern C`) call-seq: `uint64_t anos_reply_message(uint64_t message_cookie, uint64_t reply)`
>
> * Arguments (x86_64):
>   * `rdi` - Message capability cookie
>   * `rsi` - Reply
>
> * Modifies (x86_64):
>   * `rax` - Message cookie (_see notes_) (**C Return Value**)
>   * `r11` - trashed
>   * `rcx` - trashed
>
> Blocks: Never. Unblocks original message sender.

Replies to the message identified by the given message cookie with the
given `reply` value.

Messages **must** be replied to in order to unblock their senders.

Returns the message cookie as a convenience, strictly for local accounting
purposes. From the point of view of the system, after this function returns
the given cookie will be invalid and must not be reused.

### Call interfaces

> [!NOTE]
> See [the general syscalls documentation](Syscalls.md) for details on
> the general structure and architecture specifics of the syscall
> interface.
>
> This section will only discuss the IPC-specific parts of the interface.

Userspace code requiring IPC functionality can, as with all syscall
based functionality, elect to communicate directly to the interface
using architecture-specific syscall features, or use `libanos` to 
leverage its convenient, largely-architecture-agnostic interface.

As always, clients wishing to use IPC syscalls by any means will 
require the appropriate capabilities.

#### Direct syscalls - x86_64 example

Making a syscall directly (without `libanos` involvement) on x86_64 
systems can be accomplished via either the (**recommended**) `syscall`
"fast path":

```
    ; Call number in r9
    mov r9, %2

    ; set other registers as appropriate, e.g. if called from C with SysV ABI:
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls

    ; perform syscall
    syscall
```

Or by using the (**not-recommened!**) software interrupt "slow path"

```
    ; Call number in r9
    mov r9, %2

    ; set other registers as appropriate, e.g. if called from C with SysV ABI:
    mov r10, rcx            ; Fourth arg in SysV is rcx, but r10 in syscalls

    ; perform interrupt
    int 0x69
```

> [!NOTE]
> **Why the two paths?**
>
> The `syscall` instruction is _always_ the recommended way to call
> syscalls. The software interrupt interface is still around because it
> doesn't cost much and I haven't gotten around to removing it yet.
>
> It will not be around forever: it is x86_64-specific, objectively worse
> than the `syscall` instruction, and has no reason to exist.

### Design rationale

#### Why synchronous?

The kernel design favours simple, synchronous message-passing IPC
for the following reasons:

* Simple to use and easy to reason about (The _simplicity_ pillar from the [Three Pillars](ThreePillars.md))
* Servers can implement asynchronous messaging on top of sync where desired
* Frees clients from having to consider _some_ potential race conditions
* Promotes a concurrent programming model for userspace code
* Supports a zero-CPU-resource model for processes requiring service, or
  without work to do

In short, I believe that the synchronous model implemented by Anos,
in which sending tasks block for a reply, and receiving tasks block
for a message (with subsequent "fire-and-forget" reply mechanism)
makes for a system for which it is easier to write efficient, SMP-friendly
code, and about which one can reason, without undue attention required
on the many edge cases and race conditions that often come with asynchronous
systems.

That said, as mentioned above, where specific servers require it for
reasons of performance, efficiency, or simplicity they are free to 
implement asynchronous messaging on top of the kernel primitives, 
for example by immediately replying to all messages (e.g. an "accepted"
reply) and then later signalling callers when some other condition is met.

#### Why zero-copy?

I posit that, for messages requiring larger buffers and two-way communication,
the zero-copy architcture will generally exhibit better performance than 
a copying implementation. For disk drivers, where most requests will be 
actively fetching data in page-sized chunks, the overhead of the zero-copy
architecture (see below) will outweigh the requirement to copy large chunks
of memory, potentially multiple times, on every request.

> [!NOTE]
> Although the page table manipulation itself is relatively trivial - though
> certainly not free - to accomplish in the Anos design, such manipulation
> brings with it significant added cost in the form of TLB management, and
> especially TLB shootdown across processors (see _counter-arguments_, below).
>
> On the other hand, copying memory is also expensive, and can have negative
> effects on both TLB and regular CPU caches - especially when performed
> repeatedly and at the scale required for disk-related IPC in a typical
> system.

For small "ping"-type messages (which have been shown to represent the 
majority of messages in a typical microkernel system, outside of disk IO),
Anos provides the ability to omit the buffer entirely (which will result in
no page-table manipulation or TLB invalidation).

I believe this is a good balance between the needs of clients as the 
system is currently implemented.

**That said, this remains under review** and may well change as the system
design evolves. There are plans to change the way disk access works
(basing it on lazy loading driven by the demand paging subsystem)
and this, along with my learnings as the system is built out in other
areas, may invalidate my assumptions about zero-copy being the lower-cost
option - and in that case, a return to copying will be implemented if it's
found to make sense, or perhaps we'll support both copy and zero-copy
if the need is there.

##### The argument _against_ zero-copy

As mentioned above, zero-copy is not without cost, and while my current
position is that these costs are less than the cost of copying, I 
present here the counter-arguments that I am aware of, for reference
should we need to revisit these decisions in the future.

* In modern systems (especially where SMP gives multiple cores), manipulating 
  page tables to move pages between processes - or even to adjust read/write-ability
  of pages shared between processes - is an expensive operation.

* On x86_64, for example, a cross-core TLB shootdown will easily run into
  _thousands_ of cycles - and this cost, as well as the synchronization 
  complexity in the general case, only increases with core count.

* Although x86_64 does provide mechanisms to mitigate _some_ of this cost,
  those mechanisms vary significantly in support on other architectures.

* Additionally, those mechanisms come with their own complexities, and 
  using them safely across the board would significantly complicate 
  subsystems across the kernel, from basic address-space management to 
  scheduling.

When coupled with concerns around caching (which, to be clear, are
also present in the copying case), there is a significant chance that
the vast majority of messages that pass between Anos processes via the 
zero copy implementation will (at least when based on _safe_ page-table 
manipulations) incur significant performance cost over copying. 

This can only be partly mitigated in exchange for significantly increased 
complexity in the general case, and this does make the whole system more 
difficult to reason about, debug, and develop software for (breaching
the _Simplicity_ pillar from the [Three Pillars](ThreePillars.md)).

In contrast, for small message buffers, copying - especially when leveraging
advanced CPU features such as non-temporal stores (for cache management)
and SIMD (for optimized copying) - woule likely give us:

* Better performance, by allowing us to be smarter about TLB management
* Simpler code, with less hardware-specific assembly
* An overall safer, easier to reason about, and more reliable system

##### What about large buffers?

Although the small-buffer case is likely to cover the majority of 
use cases for IPC, in a microkernel system such as Anos there are
also important cases for IPC with larger buffers - for example file
systems, networking and graphics will all benefit from an ability
to quickly move large data buffers between processes.

These use-cases are the primary drivers for choosing a zero-copy
implementation currently.

To serve such cases in a copy-based scheme, without reintroducing
(in the general case) the issues described above, the shared memory
support in Anos is being designed to work together with synchronous
IPC to support efficient copy-free movement of large amounts of data
between processes.

Should we switch to a copy-based IPC system, with shared memory 
regions and message-passing used as a signalling mechanism, 
processes could safely share regions of memory as buffers
in which large amounts of data can be passed.

Shared memory in general is also not without its issues, not least
in the difficulties pushed onto application programmers in terms 
of correctness and provability, but due to the synchronous nature
of messaging, we can in many cases obviate the need to implement
"borrow" semantics in such regions, since threads will naturally
be blocked when their partners are unblocked and writing to these
buffers, relinquishing the "borrow" only when they elect to reply 
to the message and unblock the sender.

At the time of writing the shared memory primitives are implemented 
in-kernel, but are not yet exposed to user processes via system
calls. As the design progresses and is proven experimentally, more
of these capabilities will be exposed for user processes.

Of course, this would also leave open questions for _other_ threads
in the _same_ process, for which this approach would not solve the
general problem and would still require careful design in both the
OS usermode libraries and the user code itself.
