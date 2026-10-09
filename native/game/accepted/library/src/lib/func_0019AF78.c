#include "common/types.h"

/* Each display-list command is two consecutive words. */
typedef struct { u32 w0; u32 w1; } ShopGfx;

extern u8 D_8021A121, D_8021A122;
extern void *D_80219F48[50];
extern u8 *D_801F3614;
/* Retail publishes the write head after every eight-byte command. */
extern u32 * volatile D_800E9BA0;
extern void func_8020122C(void *resource);
extern void func_80200DD0(s32 x, s32 y, s32 size);

void func_0019AF78(s32 x, s32 y, s32 item, u32 slot)
{
    u32 first = D_8021A121;
    u32 second = D_8021A122;
    s32 index = item - 1;

    if (first == second) {
        ShopGfx *commands;
        u8 *texture;
        u32 load_block, tile, tile_size;
        func_8020122C(D_801F3614);
        load_block = 0x0707F400;
        tile = 0xF5480400;
        commands = (ShopGfx *)D_800E9BA0;
        tile_size = 0x0003C03C;
        texture = D_801F3614 + 0x200 + ((index & 0xFFFF) << 8);
        D_800E9BA0 = (u32 *)(commands + 1);
        commands[0].w0 = 0xFD500000;
        commands[0].w1 = (u32)texture;
        D_800E9BA0 = (u32 *)(commands + 2);
        commands[1].w0 = 0xF5500000;
        commands[1].w1 = 0x07000000;
        D_800E9BA0 = (u32 *)(commands + 3);
        commands[2].w0 = 0xE6000000;
        commands[2].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 4);
        commands[3].w0 = 0xF3000000;
        commands[3].w1 = load_block;
        D_800E9BA0 = (u32 *)(commands + 5);
        commands[4].w0 = 0xE7000000;
        commands[4].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 6);
        commands[5].w0 = tile;
        commands[5].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 7);
        commands[6].w0 = 0xF2000000;
        commands[6].w1 = tile_size;
    } else {
        ShopGfx *commands;
        u8 *texture;
        u32 load_block, tile, tile_size;
        u32 offset = slot & 0xFF;
        void **base = D_80219F48;
        void **resource = (void **)((u8 *)base + (offset << 2));
        func_8020122C(*resource);
        load_block = 0x0707F400;
        tile = 0xF5480400;
        commands = (ShopGfx *)D_800E9BA0;
        tile_size = 0x0003C03C;
        commands[0].w0 = 0xFD500000;
        texture = (u8 *)*resource;
        D_800E9BA0 = (u32 *)(commands + 1);
        D_800E9BA0 = (u32 *)(commands + 2);
        commands[1].w0 = 0xF5500000;
        commands[1].w1 = 0x07000000;
        D_800E9BA0 = (u32 *)(commands + 3);
        commands[2].w0 = 0xE6000000;
        commands[2].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 4);
        commands[3].w0 = 0xF3000000;
        commands[3].w1 = load_block;
        D_800E9BA0 = (u32 *)(commands + 5);
        commands[4].w0 = 0xE7000000;
        commands[4].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 6);
        commands[5].w0 = tile;
        commands[5].w1 = 0;
        D_800E9BA0 = (u32 *)(commands + 7);
        commands[6].w0 = 0xF2000000;
        commands[6].w1 = tile_size;
        commands[0].w1 = (u32)(texture + 0x200);
    }
    func_80200DD0(x, y, 27);
}
