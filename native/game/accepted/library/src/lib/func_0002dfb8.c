typedef unsigned char u8;
typedef unsigned int u32;
typedef int s32;

extern u32 D_800ABD74;
extern u32 D_800ABD70;
extern u32 D_800C47E0;
extern u8 resource_archive_rom_base[];
extern void func_0001A380(u32 rom_address, void *destination, u32 bytes);
extern void *func_80070F30(u32 bytes);

void *func_0002dfb8(void *buffer, u32 key)
{
    /* Preserve the 40-byte local DMA region at sp+0x10..sp+0x38. */
    u8 dma_scratch[40];
    u8 *source;
    /* The cursor and cached length occupy successive phases of the same word.
     * Keeping that local lifetime reproduces the original allocation. */
    union { u8 *cursor; u32 length; } work;
    register u32 transfer_length;
    u32 key_mask;
    u32 value;
    u32 rom_address;
    s32 payload_remaining;
    s32 remaining;

    if (key == 0) {
        work.length = 0;
    } else if (key == D_800ABD74) {
        work.length = D_800ABD70;
    } else {
        key_mask = 0x0FFFFFFF;
        source = (u8 *)((((u32)dma_scratch + 15) >> 4) << 4);
        work.cursor = (u8 *)&D_800C47E0;
        func_0001A380((u32)(resource_archive_rom_base + (key & key_mask)), source, 4);
        remaining = 3;
        do {
            *work.cursor++ = *source++;
            remaining--;
        } while (remaining != -1);
        value = D_800C47E0;
        D_800ABD74 = key;
        work.length = value;
        D_800ABD70 = work.length;
    }

    if (buffer == 0) buffer = func_80070F30(work.length);
    source = (u8 *)((((u32)dma_scratch + 15) >> 4) << 4);
    transfer_length = (work.length + 1) & ~1U;
    rom_address = (u32)(resource_archive_rom_base + 4 + key);
    work.cursor = buffer;
    if (transfer_length < 16) {
        func_0001A380(rom_address, source,
                     (transfer_length + 1) & ~1U);
        payload_remaining = transfer_length - 1;
        if (transfer_length != 0) {
            do {
                *work.cursor++ = *source++;
                payload_remaining--;
            } while (payload_remaining != -1);
        }
    } else {
        func_0001A380(rom_address, buffer,
                     (transfer_length + 1) & ~1U);
    }
    return buffer;
}

/* Independently callable four-word body at owner offset 0x170. */
static u32 func_0002E128(u32 key)
{
    return (u32)(resource_archive_rom_base + 4 + key);
}
