#include "game/cutscene_panels.h"

extern void func_000523b4(s16);
extern void func_0004f2ac(const u8 *, s32, s32);
extern void func_000577a0(s32, s32);
extern Panel *func_000552c0(void);
extern void func_00291194(s32, Panel *);

void func_002978F4(Panel *panel, s32 index)
{
    /* Keep the scaled index first in the KMC address addition. */
    u8 *indexedPanel = (u8 *)((u32)(index * 2) + (u32)panel);
    s16 state = *(s16 *)(indexedPanel + 0x18);

    if (state != 0) {
        PanelContent *content = panel->content;
        s32 indicatorOffset = (s8)panel->field22 * content->field5F6 + 6;
        s32 x = panel->field2C;
        s32 y = panel->field30;
        s32 row;

        func_000523b4(state);
        for (row = 0; row < content->field5F4; row++) {
            s32 line = content->field5F1 + row;

            if (line < content->field5F5) {
                s32 distance = 6;
                s32 stride = content->field5F6;
                const u8 *text;

                distance += row * stride;
                text = panel->content->rows[line];
                func_0004f2ac(text, x + 22, y + distance);
                stride = content->field5F6;
                distance = row * stride;
                distance += 6;
                func_0004f2ac(text, x + 22, y + distance);
            }
        }
        func_000577a0(x, (y + indicatorOffset) + 2);
        if (panel == func_000552c0()) {
            panel->flags |= 8;
        } else {
            panel->flags &= ~8;
        }
        func_00291194(0, panel);
    }
}
