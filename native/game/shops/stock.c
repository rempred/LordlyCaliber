/* Adapted from accepted func_0019BE40 (original in game/accepted).
 * Preserve allocation, scenario/site lookup and archive lifetime. Authored
 * halfword rows replace the retail archive range and fixed consumables.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
extern u8 D_8018F481, D_801E7E15[];
extern u16 *D_80219F20;
extern u8 lc_shop_rows[];
extern void *resource_alloc(u32);
extern void memset_00023780(void *, int);
extern u8 *boot_command_stream_dispatch(u32, int, int, int);
extern void boot_command_stream_resource_node_dispatch(u32, int, int);
extern void lc_dma_read(u32, void *, u32);

void func_0019BE40(u32 site_index) {
    u8 *sites, *strongholds;
    u16 stronghold;
    u32 shop;
    u16 *temporary_list = resource_alloc(0x200);
    D_80219F20 = temporary_list;
    memset_00023780(temporary_list, 0x200);
    sites = boot_command_stream_dispatch(0x021AF9F8, 2, -3,
                                         D_801E7E15[12 * D_8018F481]);
    strongholds = boot_command_stream_dispatch(0x021AF9F8, 2, -3, 0);
    ++sites;
    stronghold = (u16)(((u16)sites[4 * site_index] << 8) + sites[4 * site_index + 1] - 1);
    shop = strongholds[28 * stronghold + 27];
    if (shop >= LC_SHOP_COUNT)
        shop = 0;
    lc_dma_read((u32)lc_shop_rows + 176 * shop, D_80219F20, 176);
    boot_command_stream_resource_node_dispatch(0x021AF9F8, 2, -17);
}
