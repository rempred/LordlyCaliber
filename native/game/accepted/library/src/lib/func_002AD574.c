typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

typedef struct BodyFilterRow {
    u8 pad00[0xF6];
    u8 memberIndex;
} BodyFilterRow;

extern u8 D_8022A7F0;
extern u8 D_801976DC;
extern u8 D_801976E8;
extern u16 D_80193BD8[];
extern u8 D_80193BF3[];
extern u16 D_80190EBC[];
extern u8 D_80191010[];
extern u16 D_80195578[];
extern u8 D_80195593[];
extern u16 D_8019532C[];
extern u8 D_80195480[];
extern s32 func_0020C014(void *unit);

u8 func_002AD574(void *unit)
{
    BodyFilterRow *row = unit;
    u8 context;
    u8 excluded;
    s32 rosterBelow30;
    u8 member;
    s32 offset;
    s32 blocked;

    /* Keep this byte load in the folded entry. */
    context = D_8022A7F0;
    excluded = 0;
    if (context == 1) {
        return 0;
    }
    if (func_0020C014(unit) != 0) {
        rosterBelow30 = D_801976DC < 30;
    } else {
        rosterBelow30 = D_801976E8 < 30;
    }
    if (rosterBelow30) {
        member = row->memberIndex;
        if (member < 100) {
            offset = member * 0x38;
            if (*(u16 *)((u8 *)D_80193BD8 + offset) == 0) {
                excluded = 1;
            }
            blocked = D_80193BF3[offset] & 4;
        } else {
            if (D_80190EBC[member] == 0) {
                excluded = 1;
            }
            blocked = D_80191010[member] & 4;
        }
    } else {
        member = row->memberIndex;
        if (member < 100) {
            offset = member * 0x34;
            if (*(u16 *)((u8 *)D_80195578 + offset) == 0) {
                excluded = 1;
            }
            blocked = D_80195593[offset] & 4;
        } else {
            if (D_8019532C[member] == 0) {
                excluded = 1;
            }
            blocked = D_80195480[member] & 4;
        }
    }
    if (blocked != 0) {
        excluded = 1;
    }
    return excluded;
}
