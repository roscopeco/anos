global initial_server_loader

extern initial_server_loader_bounce

initial_server_loader:
    ; NOTE: The '4' here must stay in sync with INIT_STACK_STATIC_VALUE_COUNT from loader.h
    ; The 8 is just sizeof(uintptr_t)
    ;
    mov rsi,[rsp+4*8]                           ; get executable name from argv[0]
    jmp initial_server_loader_bounce            ; let's bounce...

