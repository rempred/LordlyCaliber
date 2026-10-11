typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

/* Only the observed cache words and first header word are viewed here. */
extern u32 D_800ABD74;
extern u32 D_800ABD70;
extern u32 D_800C47E0;
extern u8 resource_archive_rom_base[];
extern void func_0001A380(u32 rom_address, void *destination, u32 bytes);

static inline u32 resource_header_word(u32 key)
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

extern void *func_80071C04(u32 bytes);
extern void *func_80070F30(u32 bytes);
extern void *func_80071288(u32 bytes);
extern void *func_8009DBB8(void *, u32);
extern u32 func_8007A7E0(void *);
extern void func_8007A110(void *, void *);
extern void func_800712C4(void *);

void *func_0002e348(u32 key)
{
    void *compressed;
    void *decoded;
    compressed = func_80071C04(resource_header_word(key));
    func_8009DBB8(compressed, key);
    decoded = func_80071C04(func_8007A7E0(compressed));
    func_8007A110(decoded, compressed);
    func_800712C4(compressed);
    return decoded;
}
