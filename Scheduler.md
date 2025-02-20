## Scheduler

### High-level

The scheduler currently used in the Stage3 kernel is a per-core
prioritised round-robin scheduler with four priority classes and
up to 255 fine priority levels within each class.

The scheduler currently operates only at the granularity of tasks -
it neither knows nor particularly cares about processes.

This means:

* There are four high-level priority "classes":
  * `TASK_CLASS_REALTIME`
  * `TASK_CLASS_HIGH`
  * `TASK_CLASS_NORMAL`
  * `TASK_CLASS_IDLE`
* Each CPU core has its own scheduler that runs _mostly_ independently
* The scheduler is based around four queues (one for each class)
  * Each core has a completely separate set of queues
* Each core has its own scheduler lock
  * We _may_ want to make this finer-grained in future, such that each _class_ has its own lock...

I'm still tuning and tinkering with the specific priority rules in 
the scheduler, so they are not _yet_ documented here. See `sched/prr.c`
for the code. Once things are more settled I'll document them here 
in detail.

### System tick

The scheduler is driven by the system tick, and runs at `KERNEL_HZ`.
Each core's local APIC timer is used to drive this, such that each
core's scheduler runs independently of the others.

### Balancing / Rebalancing

Currently, when the scheduler runs on a given CPU due to the system
tick, it will **never** rebalance tasks between CPUs - it will always:

* Pick the next task (according to the rules) from its own set of queues, and schedule it
* Return the previously running task to its own queues (unless the task is no longer runnable)

Load balancing / rebalancing is **only** carried out when:

* A thread is first scheduled
* A thread is woken from a `sleep` call

The latter one _seems_ reasonable - if a task has been asleep, it
_feels like_ there's a decent chance of the CPU caches having moved
on since it was last run - but this of course won't work for all 
types of load - if indeed it holds true for _any_ - so will need 
some analysis once things are more settled.

It works nicely though _for now_.

### Locking

The scheduler has some fairly specific locking requirements, and so it 
implements its own locks (in `sched_lock` and `sched_unlock`). Most of 
the `sched` routines require the caller to manage locking using those two
routines (see comments at `sched_schedule` callsites for example).

Particularly when moving things around in scheduler queues, the lock
must be held. If this is for the current CPU, then interrupts must
also be disabled.

