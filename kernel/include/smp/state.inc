;
; * stage3 - SMP per-CPU state (assembly edition)
; * anos - An Operating System
; *
; * Copyright (c) 2025 Ross Bamford
; * 
; * Be sure to keep this in-sync with state.h!
;

%define CPU_TASK_CURRENT     928 
%define CPU_TASK_TSS         936
%define CPU_RSP_STASH        1024