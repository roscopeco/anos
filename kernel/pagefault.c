#include <stdint.h>

#include "debugprint.h"
#include "printhex.h"
#include "machine.h"

#define IS_KERNEL_CODE(addr)    (( (addr & 0xFFFFFFFF80000000) != 0 ))

static inline void debug_page_fault_code(uint8_t code) {
    debugstr(" [");
    debugstr(code & 0x8000 ? "SGX|" : "sgx|");
    debugstr(code & 0x40   ?  "SS|" :  "ss|");
    debugstr(code & 0x20   ?  "PK|" :  "pk|");
    debugstr(code & 0x10   ?   "I|" :   "i|");
    debugstr(code & 0x8    ?   "R|" :   "r|");
    debugstr(code & 0x4    ?   "U|" :   "u|");
    debugstr(code & 0x2    ?   "W|" :   "w|");
    debugstr(code & 0x1    ?   "P"  :   "p" );
    debugstr("]");
}

static inline void debug_page_fault(uint64_t code, uintptr_t fault_addr, uintptr_t origin_addr) {
    debugattr(0x4C);
    debugstr("PANIC");
    debugattr(0x0C);
    debugstr("         : Unhandled page fault (0x0e)");
    debugstr("\nCode          : ");
    printhex64(code, debugchar);
    debug_page_fault_code(code);
    debugstr("\nOrigin IP     : ");
    printhex64(origin_addr, debugchar);
    debugstr(IS_KERNEL_CODE(origin_addr) ? " [KERNEL]" : " [NON-KERNEL]");
    debugstr("\nFault addr    : ");
    printhex64(fault_addr, debugchar);

    halt_and_catch_fire();
}

void handle_page_fault(uint64_t code, uint64_t fault_addr, uint64_t origin_addr) {
    debug_page_fault(code, fault_addr, origin_addr);
}
