#include "common/types.h"

typedef struct BriefingRow {
    void *components[3];
    u8 pad0C[0x2A];
    u16 field36;
    u16 field38;
    u16 field3A;
    u16 field3C;
    u8 pad3E[0x0A];
    s32 field48;
    s32 field4C;
    u8 pad50[8];
    s32 field58;
    u8 pad5C[0x9C];
} BriefingRow;

typedef struct BriefingActor BriefingActor;

typedef struct BriefingScene {
    u8 pad00[0x18];
    BriefingActor *actors[28];
} BriefingScene;

extern u8 *D_801CE8BC;
extern BriefingScene *D_8022A974;
extern s32 func_0020BFF8(void *row);
extern s32 func_0020C0E8(void *row);
extern void func_002B867C(BriefingRow *row, s32 rowIndex, s32 mode, s32 field48,
                         s32 field4C, u8 anchor, s16 animation);

s32 func_002B88C8(s16 anchor, s16 animation)
{
    s32 rowIndex = 0;
    s32 anchorOffset = anchor * (s32)sizeof(BriefingActor *);
    u8 anchorByte = (u8)anchor;

    do {
        BriefingRow *row = (BriefingRow *)(D_801CE8BC + 0x1C4 + rowIndex * (s32)sizeof(BriefingRow));

        if (row->field48 != 0 && func_0020BFF8(row) == 0 &&
            func_0020C0E8(row) != 0 &&
            *(BriefingActor **)((u8 *)D_8022A974->actors + anchorOffset) != 0) {
            func_002B867C(row, rowIndex, row->field58 < 4, row->field48,
                          row->field4C, anchorByte, animation);
            return 0;
        }
        rowIndex++;
    } while (rowIndex < 20);

    return 0;
}
