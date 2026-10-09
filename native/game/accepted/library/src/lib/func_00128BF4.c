typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define RECORD_FIELD(record, type, offset) (*(type *)((u8 *)(record) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern void func_001072B8(RuntimeUnit *unit);

void func_00128BF4(RuntimeUnit *unit, s32 index, s32 mode)
{
    u32 flags;

    UNIT_FIELD(u32, 0x00) |= 0x2000;
    func_001072B8(unit);
    if (mode == 0) {
        u8 field91 = UNIT_FIELD(u8, 0x91);
        f32 field18 = UNIT_FIELD(f32, 0x18);

        UNIT_FIELD(u8, 0x98) = field91;
        UNIT_FIELD(f32, 0x9C) = field18;
    }

    flags = UNIT_FIELD(u32, 0x00);
    /* Keep this assignment before the byte/word literals: its shorter
       compiler lifetime restores the retail saved-register allocation.
       The pinned scheduler emits the store in its retail instruction position. */
    UNIT_FIELD(s32, 0x84) = index;
    UNIT_FIELD(u8, 0x91) = 1;
    UNIT_FIELD(u8, 0x20) = 1;
    UNIT_FIELD(s32, 0x24) = 1;
    flags &= ~4;
    flags &= ~0x200;
    UNIT_FIELD(u32, 0x00) = flags;

    if (mode == 0) {
        f32 field08 = UNIT_FIELD(f32, 0x08);
        f32 field0C = UNIT_FIELD(f32, 0x0C);
        f32 field10 = UNIT_FIELD(f32, 0x10);
        s32 field14 = UNIT_FIELD(s32, 0x14);

        UNIT_FIELD(f32, 0x28) = field08;
        UNIT_FIELD(f32, 0x2C) = field0C;
        UNIT_FIELD(f32, 0x30) = field10;
        UNIT_FIELD(s32, 0x58) = field14;
    }

    UNIT_FIELD(f32, 0x4C) = RECORD_FIELD(D_801F0CB0[UNIT_FIELD(s32, 0x84)], f32, 0x08);
    UNIT_FIELD(f32, 0x50) = RECORD_FIELD(D_801F0CB0[UNIT_FIELD(s32, 0x84)], f32, 0x0C);
    UNIT_FIELD(f32, 0x54) = RECORD_FIELD(D_801F0CB0[UNIT_FIELD(s32, 0x84)], f32, 0x10);
    UNIT_FIELD(s32, 0x64) = RECORD_FIELD(D_801F0CB0[UNIT_FIELD(s32, 0x84)], s32, 0x14);
}
