typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern s8 D_801F1039[];
extern u16 D_801F1042[];
extern u8 D_801F1052[];
extern void func_001072B8(RuntimeUnit *unit);

void func_00118D0C(RuntimeUnit *unit)
{
    s32 first = 0;
    s32 index;
    /* Selecting both bounds retains the observed register loop limit. */
    s32 limit;
    if (FIELD(unit, u32, 0x00) & 8) {
        first = 30;
        limit = 50;
    } else {
        limit = 50;
    }

    for (index = first; index < limit; index++) {
        RuntimeUnit *other = D_801F0CB0[index];
        if (first != 0) {
            if (FIELD(other, u32, 0x00) & 8)
                continue;
        } else if (!(FIELD(other, u32, 0x00) & 8)) {
            continue;
        }
        if (FIELD(other, s32, 0x84) == FIELD(unit, u8, 0x04)) {
            FIELD(other, s32, 0x84) = -1;
            func_001072B8(other);
        }
    }

    /* The signed entry comparison preserves the observed -1 sentinel. */
    for (index = 0; index < 8; index++) {
        if (D_801F1039[index] == FIELD(unit, u8, 0x04))
            break;
    }
    if (index != 8) {
        D_801F1039[index] = -1;
        D_801F1042[index] = 0xFFFF;
        D_801F1052[index] = 0;
    }
}
