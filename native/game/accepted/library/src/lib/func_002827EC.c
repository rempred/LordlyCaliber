typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;
typedef float f32;
typedef double f64;
typedef s32 M2C_UNK;
typedef s8 M2C_UNK8;
typedef s16 M2C_UNK16;
typedef s32 M2C_UNK32;
typedef s64 M2C_UNK64;
M2C_UNK func_0002de50(M2C_UNK, M2C_UNK, s32);                   
M2C_UNK func_002836B0(M2C_UNK);                                 
s32 func_00283E14(s16);                                         
M2C_UNK func_80076F5C(M2C_UNK, void *, M2C_UNK, M2C_UNK, M2C_UNK, M2C_UNK, M2C_UNK);
M2C_UNK func_8024026C();                                        
M2C_UNK memset_00023780(M2C_UNK, s32);                          
M2C_UNK os_inval_dcache(M2C_UNK, s32);                          
M2C_UNK os_inval_icache(M2C_UNK, s32);                          

extern u8 overlay_286bd0_icache_text_start[];
extern u8 overlay_286bd0_icache_data_start[];
extern u8 overlay_286bd0_dcache_data_start[];
extern u8 overlay_286bd0_dcache_bss_start[];
extern u8 overlay_286bd0_dma_rom_start[];
extern u8 overlay_286bd0_dma_rom_end[];
extern u8 overlay_286bd0_dma_text_start[];
extern u8 overlay_286bd0_bss_start[];
extern u8 overlay_286bd0_bss_end[];
extern u8 overlay_29a4c0_icache_text_start[];
extern u8 overlay_29a4c0_icache_data_start[];
extern u8 overlay_29a4c0_dcache_data_start[];
extern u8 overlay_29a4c0_dcache_bss_start[];
extern u8 overlay_29a4c0_dma_rom_start[];
extern u8 overlay_29a4c0_dma_rom_end[];
extern u8 overlay_29a4c0_dma_text_start[];
extern u8 overlay_29a4c0_bss_start[];
extern u8 overlay_29a4c0_bss_end[];
extern u8 overlay_2a8d20_icache_text_start[];
extern u8 overlay_2a8d20_icache_data_start[];
extern u8 overlay_2a8d20_dcache_data_start[];
extern u8 overlay_2a8d20_dcache_bss_start[];
extern u8 overlay_2a8d20_dma_rom_start[];
extern u8 overlay_2a8d20_dma_rom_end[];
extern u8 overlay_2a8d20_dma_text_start[];
extern u8 overlay_2a8d20_bss_start[];
extern u8 overlay_2a8d20_bss_end[];
extern u8 overlay_2ae3c0_icache_text_start[];
extern u8 overlay_2ae3c0_icache_data_start[];
extern u8 overlay_2ae3c0_dcache_data_start[];
extern u8 overlay_2ae3c0_dcache_bss_start[];
extern u8 overlay_2ae3c0_dma_rom_start[];
extern u8 overlay_2ae3c0_dma_rom_end[];
extern u8 overlay_2ae3c0_dma_text_start[];
extern u8 overlay_2ae3c0_bss_start[];
extern u8 overlay_2ae3c0_bss_end[];
extern u8 resource_80225A1C[];
extern u8 resource_80225C08[];
extern u8 resource_80225F40[];
extern u8 resource_802260F0[];
extern u8 resource_80226324[];
extern u8 resource_8022643C[];
extern u8 D_800E7A33;

void func_002827EC(s32 arg0, s32 arg1) {
    M2C_UNK var_a1_106;
    s32 var_s0_9;
    s32 var_v1_10;
    u8 *temp_t0;
    u32 temp_v1_flag;

    var_s0_9 = arg0;
    var_v1_10 = 0;
    if (arg1 == -1) {
        goto block_2;
    }
    *(s16 *)0x8022A994 = arg1;
block_2:
    if (var_s0_9 >= -2) {
        goto block_4;
    }
    var_v1_10 = func_00283E14(*(s16 *)0x8022A994);
block_4:
    if (var_v1_10 == 0) {
        goto block_6;
    }
    var_s0_9 = var_v1_10;
    goto block_8;
block_6:
block_8:
    if (var_s0_9 == -2) {
        goto block_10;
    }
    *(s8 *)0x8022AC80 = var_s0_9;
block_10:
    if ((u32) (var_s0_9 + 0xA) >= 9U) {
        goto block_44;
    }
    switch (var_s0_9) {
    case -9:
        os_inval_icache(overlay_29a4c0_icache_text_start, overlay_29a4c0_icache_data_start - overlay_29a4c0_icache_text_start);
        os_inval_dcache(overlay_29a4c0_dcache_data_start, overlay_29a4c0_dcache_bss_start - overlay_29a4c0_dcache_data_start);
        func_0002de50(overlay_29a4c0_dma_rom_start, overlay_29a4c0_dma_text_start, overlay_29a4c0_dma_rom_end - overlay_29a4c0_dma_rom_start);
        if (overlay_29a4c0_bss_start != overlay_29a4c0_bss_end)
            memset_00023780(overlay_29a4c0_bss_start, overlay_29a4c0_bss_end - overlay_29a4c0_bss_start);
        os_inval_icache(overlay_2a8d20_icache_text_start, overlay_2a8d20_icache_data_start - overlay_2a8d20_icache_text_start);
        os_inval_dcache(overlay_2a8d20_dcache_data_start, overlay_2a8d20_dcache_bss_start - overlay_2a8d20_dcache_data_start);
        func_0002de50(overlay_2a8d20_dma_rom_start, overlay_2a8d20_dma_text_start, overlay_2a8d20_dma_rom_end - overlay_2a8d20_dma_rom_start);
        if (overlay_2a8d20_bss_start != overlay_2a8d20_bss_end)
            memset_00023780(overlay_2a8d20_bss_start, overlay_2a8d20_bss_end - overlay_2a8d20_bss_start);
        func_002836B0(2);
        *(s8 *)0x8022A981 = 2;
        func_80076F5C(0xC000, resource_80226324, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -8:
        os_inval_icache(overlay_286bd0_icache_text_start, overlay_286bd0_icache_data_start - overlay_286bd0_icache_text_start);
        os_inval_dcache(overlay_286bd0_dcache_data_start, overlay_286bd0_dcache_bss_start - overlay_286bd0_dcache_data_start);
        func_0002de50(overlay_286bd0_dma_rom_start, overlay_286bd0_dma_text_start, overlay_286bd0_dma_rom_end - overlay_286bd0_dma_rom_start);
        if (overlay_286bd0_bss_start != overlay_286bd0_bss_end)
            memset_00023780(overlay_286bd0_bss_start, overlay_286bd0_bss_end - overlay_286bd0_bss_start);
        func_002836B0(2);
        *(s8 *)0x8022A980 = 0;
        if (*(s16 *)0x8022A994 != 0)
            goto resource_nonzero;
        func_80076F5C(0xC000, resource_80225C08, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
resource_nonzero:
        func_80076F5C(0xC000, resource_80225F40, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -7:
        *(s8 *)0x8022A980 = 0;
        os_inval_icache(overlay_29a4c0_icache_text_start, overlay_29a4c0_icache_data_start - overlay_29a4c0_icache_text_start);
        os_inval_dcache(overlay_29a4c0_dcache_data_start, overlay_29a4c0_dcache_bss_start - overlay_29a4c0_dcache_data_start);
        func_0002de50(overlay_29a4c0_dma_rom_start, overlay_29a4c0_dma_text_start, overlay_29a4c0_dma_rom_end - overlay_29a4c0_dma_rom_start);
        if (overlay_29a4c0_bss_start != overlay_29a4c0_bss_end)
            memset_00023780(overlay_29a4c0_bss_start, overlay_29a4c0_bss_end - overlay_29a4c0_bss_start);
        os_inval_icache(overlay_2ae3c0_icache_text_start, overlay_2ae3c0_icache_data_start - overlay_2ae3c0_icache_text_start);
        os_inval_dcache(overlay_2ae3c0_dcache_data_start, overlay_2ae3c0_dcache_bss_start - overlay_2ae3c0_dcache_data_start);
        func_0002de50(overlay_2ae3c0_dma_rom_start, overlay_2ae3c0_dma_text_start, overlay_2ae3c0_dma_rom_end - overlay_2ae3c0_dma_rom_start);
        if (overlay_2ae3c0_bss_start != overlay_2ae3c0_bss_end)
            memset_00023780(overlay_2ae3c0_bss_start, overlay_2ae3c0_bss_end - overlay_2ae3c0_bss_start);
        *(s32 *)0x8022A970 = 0;
        func_002836B0(0);
        func_80076F5C(0xC000, resource_80225A1C, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -6:
        os_inval_icache(overlay_29a4c0_icache_text_start, overlay_29a4c0_icache_data_start - overlay_29a4c0_icache_text_start);
        os_inval_dcache(overlay_29a4c0_dcache_data_start, overlay_29a4c0_dcache_bss_start - overlay_29a4c0_dcache_data_start);
        func_0002de50(overlay_29a4c0_dma_rom_start, overlay_29a4c0_dma_text_start, overlay_29a4c0_dma_rom_end - overlay_29a4c0_dma_rom_start);
        if (overlay_29a4c0_bss_start != overlay_29a4c0_bss_end)
            memset_00023780(overlay_29a4c0_bss_start, overlay_29a4c0_bss_end - overlay_29a4c0_bss_start);
        os_inval_icache(overlay_2a8d20_icache_text_start, overlay_2a8d20_icache_data_start - overlay_2a8d20_icache_text_start);
        os_inval_dcache(overlay_2a8d20_dcache_data_start, overlay_2a8d20_dcache_bss_start - overlay_2a8d20_dcache_data_start);
        func_0002de50(overlay_2a8d20_dma_rom_start, overlay_2a8d20_dma_text_start, overlay_2a8d20_dma_rom_end - overlay_2a8d20_dma_rom_start);
        if (overlay_2a8d20_bss_start != overlay_2a8d20_bss_end)
            memset_00023780(overlay_2a8d20_bss_start, overlay_2a8d20_bss_end - overlay_2a8d20_bss_start);
        func_002836B0(2);
        *(s8 *)0x8022A981 = 1;
        func_80076F5C(0xC000, resource_80226324, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -4:
        *(s8 *)0x8022A980 = 0;
        os_inval_icache(overlay_29a4c0_icache_text_start, overlay_29a4c0_icache_data_start - overlay_29a4c0_icache_text_start);
        os_inval_dcache(overlay_29a4c0_dcache_data_start, overlay_29a4c0_dcache_bss_start - overlay_29a4c0_dcache_data_start);
        func_0002de50(overlay_29a4c0_dma_rom_start, overlay_29a4c0_dma_text_start, overlay_29a4c0_dma_rom_end - overlay_29a4c0_dma_rom_start);
        if (overlay_29a4c0_bss_start != overlay_29a4c0_bss_end)
            memset_00023780(overlay_29a4c0_bss_start, overlay_29a4c0_bss_end - overlay_29a4c0_bss_start);
        os_inval_icache(overlay_2ae3c0_icache_text_start, overlay_2ae3c0_icache_data_start - overlay_2ae3c0_icache_text_start);
        os_inval_dcache(overlay_2ae3c0_dcache_data_start, overlay_2ae3c0_dcache_bss_start - overlay_2ae3c0_dcache_data_start);
        func_0002de50(overlay_2ae3c0_dma_rom_start, overlay_2ae3c0_dma_text_start, overlay_2ae3c0_dma_rom_end - overlay_2ae3c0_dma_rom_start);
        if (overlay_2ae3c0_bss_start != overlay_2ae3c0_bss_end)
            memset_00023780(overlay_2ae3c0_bss_start, overlay_2ae3c0_bss_end - overlay_2ae3c0_bss_start);
        func_8024026C();
        func_80076F5C(0xC000, resource_80225A1C, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -10:
    case -3:
        *(s8 *)0x8022A980 = 0;
        os_inval_icache(overlay_29a4c0_icache_text_start, overlay_29a4c0_icache_data_start - overlay_29a4c0_icache_text_start);
        os_inval_dcache(overlay_29a4c0_dcache_data_start, overlay_29a4c0_dcache_bss_start - overlay_29a4c0_dcache_data_start);
        func_0002de50(overlay_29a4c0_dma_rom_start, overlay_29a4c0_dma_text_start, overlay_29a4c0_dma_rom_end - overlay_29a4c0_dma_rom_start);
        if (overlay_29a4c0_bss_start != overlay_29a4c0_bss_end)
            memset_00023780(overlay_29a4c0_bss_start, overlay_29a4c0_bss_end - overlay_29a4c0_bss_start);
        os_inval_icache(overlay_2a8d20_icache_text_start, overlay_2a8d20_icache_data_start - overlay_2a8d20_icache_text_start);
        os_inval_dcache(overlay_2a8d20_dcache_data_start, overlay_2a8d20_dcache_bss_start - overlay_2a8d20_dcache_data_start);
        func_0002de50(overlay_2a8d20_dma_rom_start, overlay_2a8d20_dma_text_start, overlay_2a8d20_dma_rom_end - overlay_2a8d20_dma_rom_start);
        if (overlay_2a8d20_bss_start != overlay_2a8d20_bss_end)
            memset_00023780(overlay_2a8d20_bss_start, overlay_2a8d20_bss_end - overlay_2a8d20_bss_start);
        func_002836B0(2);
        func_80076F5C(0xC000, resource_802260F0, 0, 0, 0x13F, 0xEF, 0x12C);
        break;
    case -2:
        temp_t0 = &D_800E7A33;
        temp_v1_flag = *temp_t0;
        *temp_t0 = temp_v1_flag | 8;
        func_80076F5C(0xC000, resource_8022643C, 0, 0, 0x13F, 0xEF, 0xFFFF);
        break;
    }
block_44:
    return;
}
