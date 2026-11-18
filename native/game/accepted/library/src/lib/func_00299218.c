#include "game/cutscene_panels.h"

typedef struct PanelRect {
    s16 left, top, right, bottom;
} PanelRect;

extern void func_0028F918(s32);

s32 func_00299218(Panel *panel)
{
    PanelRect rect;
    s8 selected = panel->field24;
    s32 right, bottom;

    if (selected == -1) {
        return -1;
    }
    panel->field22 = selected;
    /* The position producer selects the same four rectangles as the constructor. */
    switch (D_8023DE30->position) {
    case 0:
        rect.left = 0x9E;
        rect.top = 0x9B;
        rect.right = 0x126;
        rect.bottom = 0xCD;
        break;
    case 1:
        rect.left = 0x1C;
        rect.top = 0x9B;
        rect.right = 0x1C;
        rect.bottom = 0xCD;
        break;
    case 2:
        rect.left = 0x9E;
        rect.top = 0x1D;
        rect.right = 0x126;
        rect.bottom = 0x1D;
        break;
    case 3:
        rect.left = 0x1C;
        rect.top = 0x1D;
        rect.right = 0x1C;
        rect.bottom = 0x1D;
        break;
    }
    right = rect.right;
    bottom = rect.bottom;
    panel->field4C = right;
    panel->field50 = bottom;
    panel->field54 = right;
    panel->field58 = bottom;
    func_0028F918(0);
    return (s8)panel->field22;
}
