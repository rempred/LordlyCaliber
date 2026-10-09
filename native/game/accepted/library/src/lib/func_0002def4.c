typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

/* Only the observed cache words and first header word are viewed here. */
extern u32 D_800ABD74;
extern u32 D_800ABD70;
extern u32 D_800C47E0;
extern u8 resource_archive_rom_base[];
extern void func_0001A380(u32 rom_address, void *destination, u32 bytes);

u32 func_0002def4(u32 key)
{
    u8 dma_scratch[32];
    u8 *source;
    u8 *destination;
    s32 remaining;
    u32 value;
    u32 key_mask;

    if (key == 0) return 0;
    if (key == D_800ABD74) return D_800ABD70;

    key_mask = 0x0FFFFFFF;
    /* The original rounds its stack buffer up to a sixteen-byte boundary. */
    source = (u8 *)((((u32)dma_scratch + 15) >> 4) << 4);
    destination = (u8 *)&D_800C47E0;
    func_0001A380((u32)(resource_archive_rom_base + (key & key_mask)), source, 4);
    remaining = 3;
    do {
        *destination++ = *source++;
        remaining--;
    } while (remaining != -1);

    value = D_800C47E0;
    D_800ABD74 = key;
    D_800ABD70 = value;
    return value;
}
