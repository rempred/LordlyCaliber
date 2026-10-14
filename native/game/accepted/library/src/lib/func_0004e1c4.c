typedef unsigned char u8;
typedef signed int s32;
extern void os_inval_icache(void *, s32);
extern void os_inval_dcache(void *, s32);
extern void func_0002de50(void *, void *, s32);
extern void func_80093380(void *, s32);
extern void func_801D6280(void);
extern void func_801D3FEC(u8);
extern void func_80073164(s32, s32, s32, s32, s32, s32, s32);
extern u8 D_8018FAE0, D_8018FBDC;

/* Independently referenced linker boundaries may have the same address. */
extern u8 cold_2447a0_icache_text_start[], cold_2447a0_icache_data_start[];
extern u8 cold_2447a0_dcache_data_start[], cold_2447a0_dcache_bss_start[];
extern u8 cold_2447a0_rom_start[], cold_2447a0_rom_end[], cold_2447a0_dma_text_start[];
extern u8 cold_2447a0_bss_start[], cold_2447a0_bss_end[];

void func_0004e1c4(void)
{
    s32 one;

    os_inval_icache(cold_2447a0_icache_text_start, cold_2447a0_icache_data_start - cold_2447a0_icache_text_start);
    os_inval_dcache(cold_2447a0_dcache_data_start, cold_2447a0_dcache_bss_start - cold_2447a0_dcache_data_start);
    func_0002de50(cold_2447a0_rom_start, cold_2447a0_dma_text_start, cold_2447a0_rom_end - cold_2447a0_rom_start);
    if (cold_2447a0_bss_start != cold_2447a0_bss_end) {
        func_80093380(cold_2447a0_bss_start, cold_2447a0_bss_end - cold_2447a0_bss_start);
    }
    one = 1;
    if (D_8018FAE0 == one) {
        func_801D6280();
    } else {
        func_801D3FEC(D_8018FBDC);
        func_80073164(0, 2, one, 0x20, one, 0x800, 0x2000);
    }
}
