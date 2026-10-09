#include "common/types.h"

typedef struct { u32 w0, w1; } ShopGfx;
typedef struct {
    u8 gap00[0x22];
    s8 field22;
    u8 gap23[9];
    s32 field2C, field30;
} ShopWindowView;
extern u32 *D_800E9BA0;
extern void *D_8021A134, *D_8021A130;
extern s32 D_801F3658;
extern u8 D_801971F2[][25], D_80193BC0[][56];
extern u8 D_80219DB8[], D_80219C28[][2];
extern void *D_80219C5C[];
extern void func_8017C4B4(s32 value);
extern void func_8020122C(void *data);
extern void func_80200DD0(s32 x, s32 y, s32 size);
extern void func_80203558(s32 x0, s32 y0, s32 x1, s32 y1);
extern void func_80093380(void *data, s32 size);
extern void func_800934B0(void *data, void *format, void *text);
extern void func_80179034(void *text, s32 x, s32 y);

#define COMMAND(a,b) { \
    ShopGfx *g = (ShopGfx *)D_800E9BA0; \
    D_800E9BA0 = (u32 *)(g + 1); \
    g->w0 = (u32)(a); g->w1 = (u32)(b); \
}

void func_0019AB44(ShopWindowView *window, u32 slot_word)
{
    s32 x, y, inner_x, inner_y;
    s32 count, start, index, line_y;
    u32 sync;
    /* Stack capacity reconstructed from the complete framed owner; the
     * observed initialization call consumes 33 bytes. */
    char buffer[50];
    slot_word = (slot_word << 1) + (u32)window;
    if (*(s16 *)(slot_word + 0x18) == 0) {
        return;
    }
    x = window->field2C;
    y = window->field30;
    /* Capture this opcode only after the coordinate reads. The pinned
     * compiler then preserves the retail saved-register and load order. */
    sync = 0xE7000000;
    {
        ShopGfx *g = (ShopGfx *)D_800E9BA0;
        s32 value;
        g->w0 = sync;
        g->w1 = 0;
        value = *(s16 *)(slot_word + 0x18);
        D_800E9BA0 = (u32 *)(g + 1);
        inner_x = x + 6;
        func_8017C4B4(value);
    }
    COMMAND(0xE3001001, 0x8000);
    func_8020122C(D_8021A134);
    inner_y = y + 6;
    COMMAND(0xD7000002, 0x80008000);
    COMMAND(0xFD500000, D_8021A130);
    COMMAND(0xF5500000, 0x07000000);
    COMMAND(0xE6000000, 0);
    COMMAND(0xF3000000, 0x073BF19A);
    COMMAND(sync, 0);
    COMMAND(0xF5480A00, 0);
    COMMAND(0xF2000000, 0x0009C0BC);
    func_80200DD0(inner_x, inner_y, 28);
    func_80203558(inner_x, inner_y, x + 46, y + 54);
    func_80093380(buffer, 33);
    func_800934B0(buffer, D_80219DB8,
                 D_80193BC0[D_801971F2[D_801F3658][0]]);
    func_80179034(buffer, x + 52, inner_y);
    count = D_80219C28[window->field22][1];
    start = D_80219C28[window->field22][0];
    index = 0;
    if (count != 0) {
        /* Retain the stable table base before initializing the line offset. */
        void **texts = D_80219C5C;
        line_y = 22;
        do {
            func_80179034(texts[start + index], x + 52, y + line_y);
            line_y += 16;
            index++;
        } while (index < count);
    }
}






