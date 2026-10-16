#include "common/types.h"

typedef struct {
    u16 item;
    u8 field02;
    u8 field03;
} EncodedItemEntry;

extern EncodedItemEntry D_80196B00[];
extern EncodedItemEntry D_80193AC0[];
extern char D_801EE8C8[];
extern void func_80093540(const char *, ...);

void func_00130E60(u16 code, u8 mode)
{
    u16 item = code & 0x7FFF;
    s32 index;

    /* Explicit read/add/write avoids the pinned compiler's duplicate byte-field
     * cursors and extra reload for postfix increment. Narrowing still wraps
     * at eight bits before the following clamps. */
    if (code & 0x8000) {
        for (index = 0; index < 0x116; index++) {
            if (D_80196B00[index].item == item) {
                D_80196B00[index].field03 = D_80196B00[index].field03 + 1;
                if (D_80196B00[index].field03 >= 100) {
                    D_80196B00[index].field03 = 99;
                }
                break;
            }
        }
        if (index != 0x116) {
            return;
        }
        for (index = 0; index < 0x116; index++) {
            if (D_80196B00[index].item == 0) {
                D_80196B00[index].item = item;
                D_80196B00[index].field03 = 1;
                break;
            }
        }
        if (index == 0x116) {
            func_80093540(D_801EE8C8, item);
        }
    } else {
        for (index = 0; index < 40; index++) {
            if (D_80193AC0[index].item == item) {
                D_80193AC0[index].field03 = D_80193AC0[index].field03 + 1;
                if (mode) {
                    D_80193AC0[index].field02 = D_80193AC0[index].field02 + 1;
                }
                if (D_80193AC0[index].field03 >= 100) {
                    D_80193AC0[index].field03 = 99;
                }
                if (D_80193AC0[index].field03 < D_80193AC0[index].field02) {
                    D_80193AC0[index].field02 = D_80193AC0[index].field03;
                }
                break;
            }
        }
        if (index != 40) {
            return;
        }
        for (index = 0; index < 40; index++) {
            if (D_80193AC0[index].item == 0) {
                D_80193AC0[index].item = item;
                D_80193AC0[index].field03 = 1;
                if (mode) {
                    D_80193AC0[index].field02 = 1;
                }
                break;
            }
        }
        if (index == 40) {
            func_80093540(D_801EE8C8, item);
        }
    }
}
