#include "game/cutscene_panels.h"

extern u16 D_800C4B28;
extern u16 D_800E79B0;
extern u16 D_800E8100;
extern u16 D_800E8700;
extern u8 D_800EB1F0[];

extern Panel *func_000552c0(void);
extern void func_00052284(Panel *);
extern void func_00286544(void);
extern void func_800EA604(void *, s32);

s32 func_002993EC(void)
{
    /* The list head uses a halfword at +0xD0. Ordinary panels hold a content pointer there. */
    Panel *head = D_8018FDC0;
    Panel *panel;
    PanelContent *content;
    s32 result = -1;

    if (head == 0) {
        goto missingContent;
    }
    if (head->link04 == 0) {
        return -1;
    }
    panel = func_000552c0();
    if (panel == 0) {
        return -1;
    }
    content = panel->content;
    if (content != 0) {
        goto contentReady;
    }

missingContent:
    return -1;

contentReady:
    switch (content->field5F0) {
    case 0:
        if (panel->field18 != 0xFF) {
            goto done;
        }
        if ((panel->flags & 4) && content->field5FC < content->field5F4) {
            if (((u16 *)D_8018FDC0)[0xD0 / 2] % content->field5FE == 0) {
                content->field5FD++;
            }
            if (content->field5FD >= content->field5F3) {
                content->field5FD = 0;
                content->field5FC++;
            }
        }
        if (content->field5F1 != 0 && (D_800C4B28 & 0x10)) {
            content->field5F0 = 1;
        } else if (content->field5F1 + content->field5F4 < content->field5F5) {
            if (D_800E8100 & 0x8000) {
                func_800EA604(D_800EB1F0, 3);
                content->field5FC = 0;
                content->field5F1 += content->field5F4;
            }
        } else if (content->field5F7 != 0) {
            content->field5F0 = 5;
        } else {
            u16 buttons = D_800E8100;
            if (buttons & 0xC000) {
                func_800EA604(D_800EB1F0, 1);
                content->field5F0 = 3;
                result = -2;
            } else if ((buttons & 0x2020) && D_8023DE34->field64 == 1) {
                func_00286544();
            }
        }
        if (content->field5F2 < content->field5F1) {
            content->field5F2 = content->field5F1;
        }
        return result;

    case 1: {
        u16 buttons;
        s32 oldValue;

        if (!(D_800E79B0 & 0x10)) {
            content->field5F0 = 2;
            break;
        }
        buttons = D_800E8700;
        oldValue = content->field5F1;
        if (buttons & 0x0800) {
            s32 value = oldValue - 1;
            content->field5F1 = value & (~value >> 31);
        } else if (buttons & 0x0400) {
            s32 value;
            if (content->field5F2 < oldValue + 1) {
                value = oldValue;
            } else {
                /* Retain the byte-width input for the second increment in KMC output. */
                value = (u8)oldValue + 1;
            }
            content->field5F1 = value;
        }
        if (oldValue != content->field5F1) {
            func_800EA604(D_800EB1F0, 2);
        }
        return result;
    }

    case 2:
        if (D_800C4B28 & 0x10) {
            content->field5F0 = 1;
            goto done;
        }
        if (content->field5F1 >= content->field5F2) {
            goto resetState;
        }
        if (((u16 *)D_8018FDC0)[0xD0 / 2] & 1) {
            func_800EA604(D_800EB1F0, 2);
            content->field5F1++;
        }
        break;
        /* This exit places the zero store in the likely branch's delay slot. */
resetState:
        content->field5F0 = 0;
        break;

    case 3:
        func_00052284(panel);
        content->field5F0++;
        break;
    }

done:
    return result;
}
