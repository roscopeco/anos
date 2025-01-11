## libanos

### Anos user-space library

This directory contains the low-level user-space library for Anos, as used by 
user-mode programs that need to interface directly with the kernel.

This builds a static library that is then linked into user mode programs
to provide the basic user-mode functionality defined in the user-mode
includes (at `../include`).

**Code in here is not allowed to directly include anything from `../kernel`!**