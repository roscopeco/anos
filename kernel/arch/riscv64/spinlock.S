/*
 * stage3 - Spinlocks for RISC-V
 * anos - An Operating System
 *
 * Copyright (c) 2024 Ross Bamford
 */

.global spinlock_init
.global spinlock_lock, spinlock_unlock
.global spinlock_lock_irqsave, spinlock_unlock_irqrestore
.global spinlock_reentrant_init
.global spinlock_reentrant_lock, spinlock_reentrant_unlock

/*
 * Initialize a spinlock
 * a0 - *lock
 */
spinlock_init:
    sd      zero, 0(a0)                    # Store 0 to lock
    ret

/*
 * Lock a spinlock
 * a0 - *lock
 */
spinlock_lock:
    li      t0, 1                          # Load 1 into t0
1:
    lr.d    t1, 0(a0)                      # Load-reserve from lock
    bnez    t1, 2f                         # If lock is held, wait
    sc.d    t1, t0, 0(a0)                  # Store-conditional 1 to lock
    bnez    t1, 1b                         # If store failed, retry
    ret
2:
    # Use a hint instruction for busy-wait loops
    .option push
    .option arch, +zihintpause
    pause                                   # Hint to CPU that this is a busy-wait loop
    .option pop
    j       1b                             # Try again

/*
 * Lock a spinlock and disable interrupts
 * a0 - *lock
 * Returns: a0 - saved interrupt state
 */
spinlock_lock_irqsave:
    csrrci  t0, sstatus, 2                 # Disable interrupts (clear SIE bit)
    li      t1, 1                          # Load 1 into t1
1:
    lr.d    t2, 0(a0)                      # Load-reserve from lock
    bnez    t2, 2f                         # If lock is held, wait
    sc.d    t2, t1, 0(a0)                  # Store-conditional 1 to lock
    bnez    t2, 1b                         # If store failed, retry
    mv      a0, t0                         # Return saved interrupt state
    ret
2:
    # Use a hint instruction for busy-wait loops
    .option push
    .option arch, +zihintpause
    pause                                   # Hint to CPU that this is a busy-wait loop
    .option pop
    j       1b                             # Try again

/*
 * Unlock a spinlock
 * a0 - *lock
 */
spinlock_unlock:
    sd      zero, 0(a0)                    # Store 0 to lock
    ret

/*
 * Unlock a spinlock and restore interrupts
 * a0 - *lock
 * a1 - saved interrupt state
 */
spinlock_unlock_irqrestore:
    sd      zero, 0(a0)                    # Store 0 to lock
    csrw    sstatus, a1                    # Restore interrupt state
    ret

/*
 * Initialize a reentrant spinlock
 * a0 - *lock
 * a1 - identity
 */
spinlock_reentrant_init:
    sd      zero, 0(a0)                    # Store 0 to lock
    sd      zero, 8(a0)                    # Store 0 to ident
    ret

/*
 * Lock a reentrant spinlock
 * a0 - *lock
 * a1 - identity
 * Returns: a0 - 1 if lock taken, 0 otherwise
 */
spinlock_reentrant_lock:
    ld      t0, 8(a0)                      # Load current owner
    beq     t0, a1, 1f                     # If we own it, return 0
    li      t0, 1                          # Load 1 into t0
2:
    lr.d    t1, 0(a0)                      # Load-reserve from lock
    bnez    t1, 3f                         # If lock is held, wait
    sc.d    t1, t0, 0(a0)                  # Store-conditional 1 to lock
    bnez    t1, 2b                         # If store failed, retry
    sd      a1, 8(a0)                      # Store our identity
    li      a0, 1                          # Return 1 (lock taken)
    ret
3:
    # Use a hint instruction for busy-wait loops
    .option push
    .option arch, +zihintpause
    pause                                   # Hint to CPU that this is a busy-wait loop
    .option pop
    j       2b                             # Try again
1:
    li      a0, 0                          # Return 0 (already owned by us)
    ret

/*
 * Unlock a reentrant spinlock
 * a0 - *lock
 * a1 - identity
 * Returns: a0 - 1 if lock released, 0 otherwise
 */
spinlock_reentrant_unlock:
    ld      t0, 8(a0)                      # Load current owner
    bne     t0, a1, 1f                     # If not owned by us, return 0
    sd      zero, 0(a0)                    # Store 0 to lock
    sd      zero, 8(a0)                    # Store 0 to ident
    li      a0, 1                          # Return 1 (unlocked)
    ret
1:
    li      a0, 0                          # Return 0 (not owned by us)
    ret 