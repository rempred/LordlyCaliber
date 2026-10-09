#include "common/types.h"

typedef struct { u32 w0, w1; } ShopGfx;
typedef struct {
    u8 gap00[0x22];
    s8 field22;
    u8 gap23[9];
    s32 field2C, field30;
} ShopWindowView;

extern u32 *D_800E9BA0;
extern u8 D_80211558[], D_80213648[], D_80219C14[];
extern void *D_8021A128;
extern u8 D_8021A124;
extern void func_8017C4B4(s32 value);
extern void func_80200F70(void *a, void *b, s32 width, s32 height, s32 mode);
extern void func_80200DD0(s32 x, s32 y, s32 size);
extern void func_8020E0F0(s32 x, s32 y, u32 value, s32 digits);
extern void func_80179034(void *text, s32 x, s32 y);

void func_0019AA04(ShopWindowView *window, u32 slot_word)
{
    s32 x, y, line_y;
    s32 value;
    ShopGfx *command;
    u32 sync;
    slot_word = (slot_word << 1) + (u32)window;
    if (*(s16 *)(slot_word + 0x18) == 0) {
        return;
    }
    x = window->field2C;
    y = window->field30;
    /* Keep opcode initialization within the consumed visible-slot path. */
    sync = 0xE7000000;
    command = (ShopGfx *)D_800E9BA0;
    command->w0 = sync;
    command->w1 = 0;
    value = *(s16 *)(slot_word + 0x18);
    D_800E9BA0 = (u32 *)(command + 1);
    line_y = y + 7;
    func_8017C4B4(value);

    {
        ShopGfx *display_command = (ShopGfx *)D_800E9BA0;
        D_800E9BA0 = (u32 *)(display_command + 1);
        display_command->w0 = 0xDE000000;
        display_command->w1 = (u32)D_80213648;
    }
    func_80200F70(D_80211558, D_8021A128, 80, 17, 2);
    func_80200DD0(x + 90, line_y, 25);
    func_80200DD0(x + ((D_8021A124 & 0x80) ? 75 : 82), y + 17, 52);
    func_8020E0F0(x + 74, line_y, D_8021A124 & 0x7F, 2);
    func_8020E0F0(x + 100, line_y, window->field22, 2);
    func_80179034(D_80219C14, x + 8, y + 4);
}

