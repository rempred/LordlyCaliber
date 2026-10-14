typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

extern u8 D_801976D8;
extern u16 D_80197B60;
extern void os_inval_icache(void *, s32);
extern void os_inval_dcache(void *, s32);
extern void func_0002de50(void *, void *, s32);
extern void func_80093380(void *, s32);
extern void func_801D0A8C(void);
extern void func_801D14E8(void);
extern void func_801D0840(void);
extern void func_801AFC0C(s32);
extern void func_80073164(s32, s32, s32, s32, s32, s32, s32);
extern void func_8007337C(void);

/* These are separate linker boundaries even where their addresses coincide.
 * The original loader subtracts boundaries and conditionally clears each BSS. */
extern u8 cold_23a3a0_icache_text_start[], cold_23a3a0_icache_data_start[];
extern u8 cold_23a3a0_dcache_bss_start[], cold_23a3a0_bss_start[], cold_23a3a0_bss_end[];
extern u8 cold_23a3a0_rom_start[], cold_23a3a0_rom_end[];
extern u8 cold_23a3a0_dcache_data_start[], cold_23a3a0_dma_text_start[];
extern u8 cold_213b10_icache_text_start[], cold_213b10_icache_data_start[];
extern u8 cold_213b10_dcache_bss_start[], cold_213b10_bss_start[], cold_213b10_bss_end[];
extern u8 cold_213b10_rom_start[], cold_213b10_rom_end[];
extern u8 cold_213b10_dcache_data_start[], cold_213b10_dma_text_start[];
extern u8 cold_22a280_icache_text_start[], cold_22a280_icache_data_start[];
extern u8 cold_22a280_dcache_bss_start[], cold_22a280_bss_start[], cold_22a280_bss_end[];
extern u8 cold_22a280_rom_start[], cold_22a280_rom_end[];
extern u8 cold_22a280_dcache_data_start[], cold_22a280_dma_text_start[];

void func_0004d634(void)
{
    if ((D_801976D8 & 8) || D_80197B60 != 0) {
        os_inval_icache(cold_23a3a0_icache_text_start, cold_23a3a0_icache_data_start - cold_23a3a0_icache_text_start);
        os_inval_dcache(cold_23a3a0_dcache_data_start, cold_23a3a0_dcache_bss_start - cold_23a3a0_dcache_data_start);
        func_0002de50(cold_23a3a0_rom_start, cold_23a3a0_dma_text_start, cold_23a3a0_rom_end - cold_23a3a0_rom_start);
        if (cold_23a3a0_bss_start != cold_23a3a0_bss_end) {
            func_80093380(cold_23a3a0_bss_start, cold_23a3a0_bss_end - cold_23a3a0_bss_start);
        }
        func_801D0A8C();
    } else if (D_801976D8 & 0x40) {
        os_inval_icache(cold_23a3a0_icache_text_start, cold_23a3a0_icache_data_start - cold_23a3a0_icache_text_start);
        os_inval_dcache(cold_23a3a0_dcache_data_start, cold_23a3a0_dcache_bss_start - cold_23a3a0_dcache_data_start);
        func_0002de50(cold_23a3a0_rom_start, cold_23a3a0_dma_text_start, cold_23a3a0_rom_end - cold_23a3a0_rom_start);
        if (cold_23a3a0_bss_start != cold_23a3a0_bss_end) {
            func_80093380(cold_23a3a0_bss_start, cold_23a3a0_bss_end - cold_23a3a0_bss_start);
        }
        func_801D14E8();
    }
    os_inval_icache(cold_213b10_icache_text_start, cold_213b10_icache_data_start - cold_213b10_icache_text_start);
    os_inval_dcache(cold_213b10_dcache_data_start, cold_213b10_dcache_bss_start - cold_213b10_dcache_data_start);
    func_0002de50(cold_213b10_rom_start, cold_213b10_dma_text_start, cold_213b10_rom_end - cold_213b10_rom_start);
    if (cold_213b10_bss_start != cold_213b10_bss_end) {
        func_80093380(cold_213b10_bss_start, cold_213b10_bss_end - cold_213b10_bss_start);
    }
    os_inval_icache(cold_22a280_icache_text_start, cold_22a280_icache_data_start - cold_22a280_icache_text_start);
    os_inval_dcache(cold_22a280_dcache_data_start, cold_22a280_dcache_bss_start - cold_22a280_dcache_data_start);
    func_0002de50(cold_22a280_rom_start, cold_22a280_dma_text_start, cold_22a280_rom_end - cold_22a280_rom_start);
    if (cold_22a280_bss_start != cold_22a280_bss_end) {
        func_80093380(cold_22a280_bss_start, cold_22a280_bss_end - cold_22a280_bss_start);
    }
    func_801D0840();
    func_801AFC0C(0);
    func_80073164(0, 16, 16, 0x200, 16, 0x800, 0x1900);
    func_8007337C();
}


