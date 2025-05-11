global initial_server_loader

extern initial_server_loader_bounce

initial_server_loader:
    pop rsi                                     ; top of stack is filename pointer (in SYSTEM memory)
    jmp initial_server_loader_bounce            ; let's bounce...

