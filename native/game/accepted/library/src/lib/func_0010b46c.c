#include "common/types.h"

extern u8 b46c_icache_start[], b46c_icache_end[];
extern u8 b46c_dcache_start[], b46c_dcache_end[];
extern u8 b46c_dma_destination[], b46c_bss_start[], b46c_bss_end[];
extern u8 D_00177ED0[], D_0017F9C0[];
extern s32 D_801F0DE0, D_801F0DE8;
extern u8 D_8018F53E, D_801F365D;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);

void func_0010B46C(void)
{
    func_800900C0(b46c_icache_start, b46c_icache_end - b46c_icache_start);
    func_80090010(b46c_dcache_start, b46c_dcache_end - b46c_dcache_start);
    func_8009DA50(D_00177ED0, b46c_dma_destination, D_0017F9C0 - D_00177ED0);
    if (b46c_bss_start != b46c_bss_end) {
        func_80093380(b46c_bss_start, b46c_bss_end - b46c_bss_start);
    }
    D_801F0DE0 = 0x29;
    D_801F0DE8 = -1;
    D_8018F53E = 1;
    D_801F365D |= 1;
}