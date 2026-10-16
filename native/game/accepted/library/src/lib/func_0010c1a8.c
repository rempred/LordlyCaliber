#include "common/types.h"

extern u8 c1a8_icache_start[], c1a8_icache_end[];
extern u8 c1a8_dcache_start[], c1a8_dcache_end[];
extern u8 c1a8_dma_destination[], c1a8_bss_start[], c1a8_bss_end[];
extern u8 D_001A9290[], D_001B2670[];
extern s32 D_801F0DE0;
extern u8 D_801F365D;

void func_800900C0(void *, s32);
void func_80090010(void *, s32);
void func_8009DA50(void *, void *, s32);
void func_80093380(void *, s32);

void func_0010C1A8(void)
{
    func_800900C0(c1a8_icache_start, c1a8_icache_end - c1a8_icache_start);
    func_80090010(c1a8_dcache_start, c1a8_dcache_end - c1a8_dcache_start);
    func_8009DA50(D_001A9290, c1a8_dma_destination, D_001B2670 - D_001A9290);
    if (c1a8_bss_start != c1a8_bss_end) {
        func_80093380(c1a8_bss_start, c1a8_bss_end - c1a8_bss_start);
    }
    D_801F0DE0 = 0x16;
    D_801F365D |= 1;
}