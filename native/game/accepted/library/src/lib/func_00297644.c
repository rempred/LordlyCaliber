#include "game/chapter_card.h"
#include "game/cutscene_panels.h"

extern void func_002918F4(u8, s32, s32, s32, s32);
extern void func_000523b4(s16);
extern u8 func_00042c30(void);
extern void func_0004f2ac(const u8 *, s32, s32);
extern void func_00052ae8(Panel *, PanelContent *);
extern void func_00293208(Panel *);

void func_00297644(Panel *panel, s32 index)
{
    /* Keep the scaled index first in the KMC address addition. */
    u8 *indexedPanel = (u8 *)((u32)(index * 2) + (u32)panel);

    func_002918F4(indexedPanel[0x19], panel->field2C, panel->field30, panel->field34, panel->field38);
    if (*(s16 *)(indexedPanel + 0x18) != 0) {
        s32 x = panel->field2C;
        s32 y = panel->field30;
        PanelContent *content = panel->content;
        s32 row;

        func_000523b4(*(s16 *)(indexedPanel + 0x18));
        if (func_00042c30() == 1) {
            u32 color = 0xC8C8C800;
            u32 combine = 0xFC11FE23;
            CombatDrawCommand *command = D_800E9BA0;
            u8 alpha;

            gTitleCardDisplayListCursor = command + 1;
            command[0].first = 0xFA000000;
            alpha = indexedPanel[0x19];
            gTitleCardDisplayListCursor = command + 2;
            command[1].first = combine;
            command[1].second = 0xFFFFF3F9;
            command[0].second = color | alpha;
        }

        for (row = 0; row < content->field5F4; row++) {
            s32 line = content->field5F1 + row;
            s32 rowX = x + 8;

            if (line < content->field5F5) {
                s32 distance = row * content->field5F6;
                const u8 *text = panel->content->rows[line];

                distance += 5;
                func_0004f2ac(text, rowX, y + distance);
            }
        }
        func_00052ae8(panel, content);
        func_00293208(panel);
    }
}
