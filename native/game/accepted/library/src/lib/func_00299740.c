#include "game/cutscene_panels.h"

extern u16 D_800E8100;
extern void *D_8018F1A4;

extern Panel *func_000552c0(void);
extern Panel *func_0005526c(u16);
extern s32 func_00055390(void);
extern void func_0005a0f4(void);
extern s32 func_00298948(Panel *);
extern s32 func_00298B70(Panel *, PanelSlot *);
extern s32 func_00299218(Panel *);
extern s32 func_002993EC(void);
extern s32 func_00283DF0(void);
extern s32 func_002953E0(s32);
extern u8 *func_0028ab74(s32);
extern void func_80093380(void *, u32);
extern void memcpy_aligned(void *, const void *, s32);

void func_00299740(void)
{
    Panel *panel;
    u8 slot;
    PanelContent *content;
    PanelSlot *record;
    s32 result;

    panel = func_000552c0();
    if (panel == 0) {
        return;
    }

    ((u8 *)D_8018FDC0)[0xD2] = 0;
    if (panel->field16 & 4) {
        panel = func_0005526c(0x100);
    }
    slot = 0;
    do {
        PanelSlot *found = D_8022A974->slots[slot];
        if (found != 0 && (content = panel->content) != 0) {
            record = found;
            if (content->key != record->key) {
                ++slot;
                continue;
            }
            switch ((u8)content->key) {
            case 4:
                D_8023DE34->field65 = 0;
                record->status = func_00298948(panel);
                break;
            case 6: {
                PanelParameters *params;
                record->status = func_00298B70(panel, D_8022A974->slots[slot]);
                content = panel->content;
                params = D_8023DE34;
                params->field1A = content->field5FC;
                params->field20 = content->field5F1;
                break;
            }
            case 11: {
                PanelParameters *params;
                record->status = func_00298B70(panel, D_8022A974->slots[slot]);
                content = panel->content;
                D_8023DE34->field67 = ((u8 *)record->field10)[content->field5FC];
                params = D_8023DE34;
                params->field1A = content->field5FC;
                params->field20 = content->field5F1;
                func_80093380(params->field24, sizeof(params->field24));
                memcpy_aligned(D_8023DE34->field24, ((const u8 **)record->field04)[content->field5FC], 0x3F);
                break;
            }
            case 8:
            case 10: {
                PanelPositionState *state = D_8023DE30;
                s32 status = -1;
                if (state->field23C == 0) {
                    record->status = func_00299218(panel);
                } else {
                    s16 position = panel->field18;
                    if (position < 0xDF) {
                        panel->field18 = position + 0x20;
                        result = -1;
                        goto storeStatus;
                    }
                    {
                        u16 buttons = D_800E8100;
                        if (buttons & 0x8000) {
                            status = 0;
                        } else if ((buttons & 0x4000) || state->field23E != state->field23D) {
                            status = func_00299218(panel);
                        }
                    }
                    result = status;
                    goto storeStatus;
                }
                break;
            }
            case 9: {
                s32 status = -1;
                if (D_800E8100 & 0xC000) {
                    status = -(panel->field18 != 0xFF);
                }
                record->status = status;
                break;
            }
            case 16: {
                PanelParameters *params;
                record->status = func_00298B70(panel, D_8022A974->slots[slot]);
                content = panel->content;
                params = D_8023DE34;
                params->field1C = content->field5FC;
                params->field22 = content->field5F1;
                if (record->status >= 0) {
                    u8 index = *func_0028ab74((u8)func_002953E0(content->field5FC));
                    func_80093380(D_8023DE34->field24, sizeof(params->field24));
                    memcpy_aligned(D_8023DE34->field24, ((const u8 **)record->field04)[index], 0x3F);
                }
                break;
            }
            case 18:
                if (D_8018F1A4 == 0 && (u8)func_00283DF0() != 1) {
                    record->status = func_002993EC();
                }
                break;
            case 5:
            case 13:
            case 14:
            case 15:
            case 17:
            case 24:
                break;
            default:
                result = func_00055390();
            storeStatus:
                record->status = result;
                break;
            }
            func_0005a0f4();
            break;
        }
        ++slot;
    } while (slot < 14);
    ((u8 *)D_8018FDC0)[0xD2] = 1;
}
