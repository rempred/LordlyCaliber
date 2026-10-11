typedef unsigned char u8;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define RECORD_FIELD(record, type, offset) (*(type *)((u8 *)(record) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern s32 func_00127D30(RuntimeUnit *unit);
extern void func_001072B8(RuntimeUnit *unit);
extern void func_001070F4(RuntimeUnit *unit);

void func_00127EAC(RuntimeUnit *unit)
{
    s32 prior_index = UNIT_FIELD(s32, 0x84);

    if (prior_index == -1 && UNIT_FIELD(s32 *, 0xA8) != 0) {
        s32 position = func_00127D30(unit);

        /* The original preserves the prior -1 value across the selector call. */
        if (position != prior_index) {
            RuntimeUnit *target;

            UNIT_FIELD(s32, 0x84) = UNIT_FIELD(s32 *, 0xA8)[position];
            func_001072B8(unit);
            target = D_801F0CB0[UNIT_FIELD(s32, 0x84)];
            UNIT_FIELD(f32, 0x4C) = RECORD_FIELD(target, f32, 0x08);
            UNIT_FIELD(f32, 0x54) = RECORD_FIELD(target, f32, 0x10);
            UNIT_FIELD(f32, 0x50) = RECORD_FIELD(target, f32, 0x0C);
            UNIT_FIELD(s32, 0x64) = RECORD_FIELD(target, s32, 0x14);
            func_001070F4(unit);
        }
    }
}
