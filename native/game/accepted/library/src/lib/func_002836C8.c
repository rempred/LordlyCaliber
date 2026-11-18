#include "game/cutscene_panels.h"


s32 func_002836C8(s32 slot)
{
    PanelSlot *record = D_8022A974->slots[slot];
    Panel *panel;
    u8 found = 0;

    if (record == 0) {
        return -5;
    }
    panel = D_8018FDC0->link04;
    while (panel != 0) {
        PanelContent *content = panel->content;
        if (content != 0 && content->key == record->key) {
            found = 1;
            break;
        }
        panel = panel->link04;
    }
    if (found != 0) {
        return record->status;
    }
    return -6;
}
