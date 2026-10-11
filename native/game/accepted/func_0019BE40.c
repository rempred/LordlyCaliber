typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;

extern u8 D_8018F481;
extern u8 D_801E7E15[];
extern u16 *D_80219F20;

extern void *resource_alloc(u32 size);
extern void memset_00023780(void *destination, s32 size);
extern u8 *boot_command_stream_dispatch(u32 key, s32 mode, s32 submode, s32 index);
extern void boot_command_stream_resource_node_dispatch(u32 key, s32 mode, s32 submode);

#define READ_BE16(bytes) ((u16)(((u16)(bytes)[0] << 8) | (bytes)[1]))

void func_0019BE40(u32 site_index) {
    u8 *site_index_table;
    u8 *stronghold_records;
    u8 *shop_archive;
    u8 *shop_range;
    u16 *combined;
    u16 stronghold_index;
    u16 start_offset;
    u16 end_offset;
    u8 shop_index;
    u32 shop_offset;
    s32 offset;
    s32 equipment_index;
    u32 site_high;
    u32 site_low;

    {
        u16 *temporary_list = resource_alloc(0x200);
        D_80219F20 = temporary_list;
        memset_00023780(temporary_list, 0x200);
    }

    site_index_table = boot_command_stream_dispatch(0x021AF9F8, 2, -3,
                                                    D_801E7E15[12 * D_8018F481]);
    stronghold_records = boot_command_stream_dispatch(0x021AF9F8, 2, -3, 0);
    shop_archive = boot_command_stream_dispatch(0x021AF9F8, 2, -3, 60);

    ++site_index_table;
    site_high = site_index_table[4 * site_index];
    site_low = site_index_table[4 * site_index + 1];
    site_high <<= 8;
    site_high += site_low;
    stronghold_index = (u16)(site_high - 1);
    shop_index = stronghold_records[28 * stronghold_index + 27];
    /* On this 32-bit target, accumulating the byte address preserves retail's
       offset-first addition without changing the archive-header lookup. */
    shop_offset = (u32)shop_index << 1;
    shop_offset += (u32)shop_archive;
    shop_range = (u8 *)shop_offset;
    start_offset = READ_BE16(shop_range);
    end_offset = READ_BE16(shop_range + 2);

    combined = D_80219F20;
    combined[0] = 1;
    combined[1] = 2;
    combined[2] = 3;
    combined[3] = 4;
    combined[4] = 5;
    combined[5] = 6;
    combined[6] = 8;

    /* Seven literal consumables precede the equipment IDs. Starting this index
       at seven makes the pinned compiler build the equipment cursor once. */
    equipment_index = 7;
    for (offset = start_offset; offset < end_offset; offset += 2, ++equipment_index) {
        u16 item = (u16)shop_archive[offset] << 8;
        /* Retail writes the high byte as a halfword before the flagged ID. */
        combined[equipment_index] = item;
        item += shop_archive[offset + 1];
        combined[equipment_index] = item | 0x8000;
    }

    boot_command_stream_resource_node_dispatch(0x021AF9F8, 2, -17);
}
