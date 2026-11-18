#include "game/cutscene_panels.h"

extern void func_00052284(Panel *);
extern void func_000521fc(Panel *);

void func_00299CA8(u8 slot, s32 mode)
{
    PanelSlot *record = D_8022A974->slots[slot];
    if (record != 0) {
        Panel *panel = record->panel;
        if (func_002836C8(slot) >= -4) {
            /* Both modes update the content captured before the transition call. */
            PanelContent *content = panel->content;
            if (content->field5F0 != 3) {
                if ((u8)mode == 0) {
                    func_00052284(panel);
                } else {
                    func_000521fc(panel);
                }
                content->field5F0 = 3;
            }
        }
    }
}
