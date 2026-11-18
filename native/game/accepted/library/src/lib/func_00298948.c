#include "game/cutscene_panels.h"

extern u8 D_8022A99A;
extern u16 D_800E8100;
extern u16 D_800E8700;
extern u8 D_800EB1F0[];

extern void func_00052284(Panel *);
extern s32 func_00045df4(s16);
extern void func_800EA604(void *, s32);

s32 func_00298948(Panel *panel)
{
    PanelContent *content;
    s32 status;
    s8 selection = (s8)panel->field24;

    status = -1;
    if (selection == (s8)panel->field22) {
        status = selection;
    }
    content = panel->content;

    switch (content->field5F0) {
    case 4:
        if (D_8022A99A != 2) {
            return status;
        }
        D_8022A99A = 0;
        content->field5F0 = 6;
        goto done;

    case 3:
        func_00052284(panel);
        content->field5F0++;
        goto done;

    case 6: {
        volatile u16 *pressed;
        u16 buttons;
        s8 previous;
        s8 nextSelection;
        PanelParameters *parameters;
        s32 selectionValue;

        if (panel->field18 != 0xFF) {
            return status;
        }
        if (D_8022A99A == 1) {
            content->field5F0 = 4;
            goto done;
        }

        /* The two tests sample the input word separately. */
        pressed = &D_800E8100;
        if (*pressed & 0x1000) {
            status = 20;
            content->field5F0 = 4;
            D_8022A99A = 5;
        }
        buttons = *pressed;
        if (buttons & 0x8000) {
            status = (s8)panel->field22;
            status += func_00045df4(1) != 1;
            func_800EA604(D_800EB1F0, 3);
            goto submitted;
        }
        if (buttons & 0x4000) {
            u8 selected = (u8)panel->field24;
            panel->field22 = selected;
            status = (s8)selected;
            func_800EA604(D_800EB1F0, 13);
        submitted:
            D_8022A99A = 5;
            content->field5F0 = 4;
            goto done;
        }

        buttons = D_800E8700;
        if (buttons & 0x0800) {
            s8 byte = panel->field22;
            s32 observed = byte;
            previous = (u8)byte;
            if (observed != 0) {
                nextSelection = (s8)previous - 1;
            } else {
                nextSelection = 0;
            }
            goto updateSelection;
        }
        if (!(buttons & 0x0400)) {
            return status;
        }
        {
            s8 byte = panel->field22;
            s32 position = byte;
            /* A direct cast loses a KMC allocation used in the retail frame. */
            s32 savedSelection = (s32)((u32)(u8)byte << 24) >> 24;
            selectionValue = (s8)panel->field25;
            position++;
            position = position < selectionValue;
            position += savedSelection;
            nextSelection = position;
            previous = savedSelection;
        }
    updateSelection:
        ((volatile Panel *)panel)->field22 = nextSelection;
        /* Compare the signed new selection with the unsigned prior byte. */
        if ((s8)nextSelection != (u8)previous) {
            func_800EA604(D_800EB1F0, 2);
        }
        parameters = D_8023DE34;
        selectionValue = (s8)panel->field22;
        if (parameters->field18 == selectionValue) {
            return status;
        }
        parameters->field18 = selectionValue;
        parameters->field1A = 0;
        parameters->field20 = 0;
        break;
    }

    default:
        return status;
    }

done:
    return status;
}
