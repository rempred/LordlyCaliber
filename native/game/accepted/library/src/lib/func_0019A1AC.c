#include "common/types.h"

typedef struct { u32 w0; u32 w1; } ShopGfx;
typedef struct {
    u8 gap00[0x2C];
    /* Preserve the observed setup reads before write-head access. */
    volatile s32 field2C, field30;
    u8 gap34[0x9C];
    u8 *fieldD0;
} ShopWindowView;

extern u32 *volatile D_800E9BA0;
extern void func_8017C4B4(s32 value);
extern void func_80179034(void *text, s32 x, s32 y);

void func_0019A1AC(ShopWindowView *window, u32 slot_word)
{
    s32 x, y, row;
    u8 *data;
    ShopGfx *command;
    s32 value;
    u32 sync = 0xE7000000;

    /* Retail consumes the slot word, then reuses its address value. */
    slot_word = (slot_word << 1) + (u32)window;
    if (*(s16 *)(slot_word + 0x18) == 0) {
        return;
    }
    x = window->field2C;
    y = window->field30;
    /* Only the captured setup read requires this ordered access. */
    data = *(u8 *volatile *)&window->fieldD0;
    command = (ShopGfx *)D_800E9BA0;
    command->w0 = sync;
    command->w1 = 0;
    value = *(s16 *)(slot_word + 0x18);
    D_800E9BA0 = (u32 *)(command + 1);
    row = 0;
    func_8017C4B4(value);

    for (; row < (s32)data[0x5F4]; row++) {
        s32 index = data[0x5F1] + row;
        if (index < (s32)data[0x5F5]) {
            s32 height = row * data[0x5F6] + 5;
            /* Retail reloads the pointer here, but retains captured limits. */
            u8 *current_data = window->fieldD0;
            func_80179034(((void **)(current_data + 0x530))[index],
                         x + 6, y + height);
        }
    }
}
