#include <stdint.h>

#include "riscv64/sbi.h"

SbiResult sbi_ecall(int ext, int fid, unsigned long arg0, unsigned long arg1,
                    unsigned long arg2, unsigned long arg3, unsigned long arg4,
                    unsigned long arg5) {
    SbiResult result;

    register uintptr_t a0 __asm__("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 __asm__("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 __asm__("a2") = (uintptr_t)(arg2);
    register uintptr_t a3 __asm__("a3") = (uintptr_t)(arg3);
    register uintptr_t a4 __asm__("a4") = (uintptr_t)(arg4);
    register uintptr_t a5 __asm__("a5") = (uintptr_t)(arg5);
    register uintptr_t a6 __asm__("a6") = (uintptr_t)(fid);
    register uintptr_t a7 __asm__("a7") = (uintptr_t)(ext);

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7)
                     : "memory");

    result.error = a0;
    result.value = a1;

    return result;
}

#define IMPL_ID_MAX ((6))
static char *fw_imp_ids[IMPL_ID_MAX + 1] = {
        "BBL", "OpenSBI", "Xvisor", "KVM", "RustSBI", "Diosix", "Coffer",
};

#ifdef DEBUG_SBI
#include "kprintf.h"

void sbi_debug_info(void) {
    char *impl_id;

    long fw_id = sbi_get_firmware_id();
    if (fw_id < 0 || fw_id > IMPL_ID_MAX) {
        impl_id = "<unknown>";
    } else {
        impl_id = fw_imp_ids[fw_id];
    }

    kprintf("RISC-V SBI v%ld.%ld [%s v%ld.%ld]",
            sbi_get_spec_version() & 0xffffff,
            (sbi_get_spec_version() & 0xf000000) >> 24, impl_id,
            (sbi_get_firmware_version() & 0xffff0000) >> 16,
            sbi_get_firmware_version() & 0xffff);
}
#endif