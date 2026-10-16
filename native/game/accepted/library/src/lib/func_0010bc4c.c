#include "common/types.h"

extern u8 bc4c_icache_start[], bc4c_icache_end[];
extern u8 bc4c_dcache_start[], bc4c_dcache_end[];
extern u8 bc4c_dma_destination[], bc4c_bss_start[], bc4c_bss_end[];
extern u8 D_001A4C10[], D_001A9290[];
extern s32 D_801F0DE0, D_801F0B94, D_801F0B98, D_801F3618, D_801F3658;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);
void func_80217E30(void);

void func_0010BC4C(void)
{
    func_800900C0(bc4c_icache_start, bc4c_icache_end - bc4c_icache_start);
    func_80090010(bc4c_dcache_start, bc4c_dcache_end - bc4c_dcache_start);
    func_8009DA50(D_001A4C10, bc4c_dma_destination, D_001A9290 - D_001A4C10);
    if (bc4c_bss_start != bc4c_bss_end) {
        func_80093380(bc4c_bss_start, bc4c_bss_end - bc4c_bss_start);
    }
    D_801F0DE0 = 0x23;
    D_801F0B94 = 0x74;
    D_801F0B98 = 0x6C;
    D_801F3618 = D_801F3658;
    func_80217E30();
}