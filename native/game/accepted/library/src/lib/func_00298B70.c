#include "game/cutscene_panels.h"

extern u16 D_800C4B28;
extern u16 D_800E8700;
extern u8 D_800EB1F0[];
extern void func_00052284(Panel *);
extern void func_800EA604(void *, s32);
extern s32 func_00295574(s32);
extern void func_002950A0(PanelContent *, u8, PanelSlot *);
extern void func_00294B8C(PanelContent *, u8, PanelSlot *);
extern void func_002956F8(PanelContent *, u8, PanelSlot *);

s32 func_00298B70(Panel *panel, PanelSlot *slot)
{
    PanelContent *content = panel->content;
    u8 refresh = 0;
    s32 status = -1;
    u16 buttons;
    u8 adjustedSelection;
    /* The signed lookup step is reused across the navigation cases. */
    s32 step;
    u8 adjustedPageSelection;
    u8 adjustedPageStart;

    switch (content->field5F0) {
    case 3:
        func_00052284(panel);
        content->field5F0++;
        goto returnStatus;

    case 6:
        if (panel->field18 != 0xFF) {
            goto returnStatus;
        }
        break;

    default:
        goto returnStatus;
    }

    buttons = D_800C4B28;
    if (buttons & 0x8000) {
        func_00052284(panel);
        /* The selection byte is unsigned. The cancel status below is signed. */
        status = content->field5FC;
        content->field5F0 = 3;
        func_800EA604(D_800EB1F0, 3);
    } else if (buttons & 0x4000) {
        func_800EA604(D_800EB1F0, 1);
        status = panel->field24;
        if (content->field5FD == 16) {
            PanelSlot *other = D_8022A974->slots[5];
            if (other != 0) {
                Panel *linked = other->panel;
                linked->field4C = 31;
                linked->field50 = 20;
                linked->field54 = 289;
                linked->field58 = 20;
            }
        }
        func_00052284(panel);
        content->field5F0 = 3;
    } else {
        buttons = D_800E8700;
        if (buttons & 0x0800) {
            u8 old = content->field5FC;
            u8 selected = old - 1;
            if (old != 0) {
                content->field5FC = selected;
                if (content->field5FD == 16) {
                    if (selected == 0) {
                        goto restoreSelection;
                    }
                    content->field5FC -= func_00295574(selected);
                    func_800EA604(D_800EB1F0, 2);
                    goto clampStart;
                }
                func_800EA604(D_800EB1F0, 2);
            }

clampStart:
            selected = content->field5FC;
            refresh = 1;
            if (content->field5F1 < selected) {
                goto checkRefresh;
            }
            content->field5F1 = selected;
            if (content->field5FD != 16) {
                goto refreshContent;
            }
            if (content->field5FC == 1) {
                goto resetStart;
            }
            goto checkRefresh;

restoreSelection:
            content->field5FC = old;
            goto clampStart;
resetStart:
            content->field5F1 = 0;
            goto refreshContent;
        } else if (buttons & 0x0400) {
            u8 selected = content->field5FC;

            if (content->field5F5 != selected + 1) {
                u8 next = selected + 1;
                content->field5FC = next;
                func_800EA604(D_800EB1F0, 2);
                if (content->field5FD == 16) {
                    content->field5FC += func_00295574(content->field5FC);
                }
            }
            {
            u8 start = content->field5F1;
            u8 visible = content->field5F4;
            if (content->field5FC - (start & 0xFF) < visible) {
                goto refreshContent;
            }
            if (content->field5F5 - visible == (start & 0xFF)) {
                goto refreshContent;
            }
            {
                u8 next = start + 1;
                content->field5F1 = next;
                if (content->field5FD != 16) {
                    goto refreshContent;
                }
                if ((s16)func_00295574(next + 6) == 0) {
                    content->field5F1 += func_00295574(content->field5FC - 1);
                }
            }
            goto refreshContent;
            }
        } else if (buttons & 4) {
            if (content->field5F1 + content->field5F4 < content->field5F5) {
                u8 selected;
                u8 start;

                func_800EA604(D_800EB1F0, 2);
                start = content->field5F1 + 1;
                selected = content->field5FC;
                content->field5F1 = start;
                if (selected < start) {
                    content->field5FC = selected + 1;
                }
                if (content->field5FD == 16) {
                    u8 current = content->field5FC;
                    step = (s32)((u32)(u16)func_00295574(current) << 16) >> 16;
                    current = content->field5FC;
                    if (content->field5F5 < current + content->field5F4 + step) {
                        adjustedSelection = current + step;
                        goto storeSelection;
                    }
                    content->field5FC = current + step;
                    content->field5F1 += step;
                }
            }
            goto refreshContent;
        } else if (buttons & 8) {
            if (content->field5F1 != 0) {
                u8 selected;
                u8 start;

                func_800EA604(D_800EB1F0, 2);
                start = content->field5F1 - 1;
                selected = content->field5FC;
                content->field5F1 = start;
                if (selected >= start + content->field5F4) {
                    content->field5FC = selected - 1;
                }
                if (content->field5FD == 16) {
                    if (content->field5FC == 0) {
                        content->field5FC++;
                    }
                    step = (s32)((u32)(u16)func_00295574(content->field5FC) << 16) >> 16;
                    if (content->field5F1 != 0) {
                        content->field5F1 -= step;
                    }
                    if (content->field5FC != 0) {
                        content->field5FC -= step;
                    }
                }
            }
            goto refreshContent;
        } else if (buttons & 1) {
            u8 limit = content->field5F5;
            u8 start = content->field5F1;
            u8 visible = content->field5F4;

            s32 total = limit & 0xFF;
            s32 first = start & 0xFF;
            s32 width = visible & 0xFF;

            if (first + width * 2 < total) {
                u8 next;
                content->field5F1 = start + visible;
                next = content->field5F1;
                if (content->field5FC < next) {
                    content->field5FC = next;
                }
                func_800EA604(D_800EB1F0, 2);
                refresh = 1;
                if (content->field5FD == 16) {
                    step = (s32)((u32)(u16)func_00295574(content->field5FC) << 16) >> 16;
                    adjustedPageSelection = content->field5FC - step;
                    adjustedPageStart = content->field5F1 - step;
                    goto storePage;
                }
            } else if (first + width < total) {
                u8 next;
                content->field5F1 = limit - visible;
                next = content->field5F1;
                if (content->field5FC < next) {
                    content->field5FC = next;
                }
                func_800EA604(D_800EB1F0, 2);
                refresh = 1;
                if (content->field5FD == 16) {
                    content->field5FC += func_00295574(content->field5FC);
                }
            } else {
                goto refreshContent;
            }
        } else if (buttons & 2) {
            u8 start = content->field5F1;
            u8 visible = content->field5F4;
            s32 first = start & 0xFF;
            if (first - visible >= 0) {
                u8 next;
                s32 rowCount;
                content->field5F1 = start - visible;
                next = content->field5F1;
                rowCount = content->field5F4;
                if (content->field5FC >= rowCount + (u8)next) {
                    content->field5FC = next + rowCount - 1;
                }
                func_800EA604(D_800EB1F0, 2);
                refresh = 1;
                if (content->field5FD == 16) {
                    step = (s32)((u32)(u16)func_00295574(content->field5FC) << 16) >> 16;
                    adjustedPageSelection = content->field5FC + step;
                    adjustedPageStart = content->field5F1 + step;
                /* Both page directions use this pair of byte stores. */
            storePage:
                    content->field5FC = adjustedPageSelection;
                    content->field5F1 = adjustedPageStart;
                    goto refreshContent;
                }
            } else {
                if (first != 0) {
                    s32 rowCount;
                    content->field5F1 = 0;
                    rowCount = content->field5F4;
                    if (content->field5FC >= rowCount) {
                        content->field5FC = rowCount - 1;
                    }
                    func_800EA604(D_800EB1F0, 2);
                    if (content->field5FD != 16) {
                        goto refreshContent;
                    }
                    refresh = 1;
                    if ((s16)func_00295574(content->field5FC) != 1) {
                        goto checkRefresh;
                    }
                    adjustedSelection = content->field5FC - 1;
                    goto storeSelection;
                }
                goto refreshContent;
            }
        }
    }
    goto checkRefresh;

storeSelection:
    content->field5FC = adjustedSelection;
refreshContent:
    refresh = 1;
checkRefresh:
    if (refresh == 1) {
        switch (content->field5FD) {
        case 6:
            func_002950A0(content, content->field5F1, slot);
            break;
        case 11:
            func_00294B8C(content, content->field5F1, slot);
            break;
        case 16:
            func_002956F8(content, content->field5F1, slot);
            break;
        }
    }

returnStatus:
    return status;
}
