typedef unsigned char u8;
typedef signed int s32;
extern void os_inval_icache(void *, s32);
extern void os_inval_dcache(void *, s32);
extern void func_0002de50(void *, void *, s32);
extern void func_80093380(void *, s32);
extern void func_801E6FB0(void);
extern void func_801EE3C8(s32);
extern void func_80073164(s32, s32, s32, s32, s32, s32, s32);
extern void func_8007337C(void);
extern s32 D_8018FB4C;

/* Keep independently referenced linker boundaries distinct, including aliases
 * at the same runtime address, as in the original loader sequence. */
extern u8 cold_213b10_icache_text_start[], cold_213b10_icache_data_start[];
extern u8 cold_213b10_dcache_data_start[], cold_213b10_dcache_bss_start[];
extern u8 cold_213b10_rom_start[], cold_213b10_rom_end[], cold_213b10_dma_text_start[];
extern u8 cold_213b10_bss_start[], cold_213b10_bss_end[];
extern u8 cold_23b220_icache_text_start[], cold_23b220_icache_data_start[];
extern u8 cold_23b220_dcache_data_start[], cold_23b220_dcache_bss_start[];
extern u8 cold_23b220_rom_start[], cold_23b220_rom_end[], cold_23b220_dma_text_start[];
extern u8 cold_23b220_bss_start[], cold_23b220_bss_end[];

void func_0004da84(void)
{
    os_inval_icache(cold_213b10_icache_text_start, cold_213b10_icache_data_start - cold_213b10_icache_text_start);
    os_inval_dcache(cold_213b10_dcache_data_start, cold_213b10_dcache_bss_start - cold_213b10_dcache_data_start);
    func_0002de50(cold_213b10_rom_start, cold_213b10_dma_text_start, cold_213b10_rom_end - cold_213b10_rom_start);
    if (cold_213b10_bss_start != cold_213b10_bss_end) {
        func_80093380(cold_213b10_bss_start, cold_213b10_bss_end - cold_213b10_bss_start);
    }
    os_inval_icache(cold_23b220_icache_text_start, cold_23b220_icache_data_start - cold_23b220_icache_text_start);
    os_inval_dcache(cold_23b220_dcache_data_start, cold_23b220_dcache_bss_start - cold_23b220_dcache_data_start);
    func_0002de50(cold_23b220_rom_start, cold_23b220_dma_text_start, cold_23b220_rom_end - cold_23b220_rom_start);
    if (cold_23b220_bss_start != cold_23b220_bss_end) {
        func_80093380(cold_23b220_bss_start, cold_23b220_bss_end - cold_23b220_bss_start);
    }
    func_801E6FB0();
    func_801EE3C8(D_8018FB4C);
    func_80073164(0, 1, 1, 0x80, 1, 0x240, 0x2000);
    func_8007337C();
}
