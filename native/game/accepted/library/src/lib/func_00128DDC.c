typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
extern void func_001289BC(RuntimeUnit *unit);
extern f32 func_00128050(RuntimeUnit *unit);
extern void func_001072B8(RuntimeUnit *unit);

void func_00128DDC(RuntimeUnit *unit)
{
    if (UNIT_FIELD(s32, 0x88) > 0) {
        func_001289BC(unit);
        UNIT_FIELD(s32, 0x88)--;
    } else {
        UNIT_FIELD(s32, 0x88) = 0x5A;
        if (func_00128050(unit) == -1.0f) {
            u32 flags;

            func_001072B8(unit);
            flags = UNIT_FIELD(u32, 0x00);
            UNIT_FIELD(u8, 0x20) = 1;
            UNIT_FIELD(s32, 0x24) = 1;
            flags &= ~4;
            flags &= ~0x200;
            UNIT_FIELD(u32, 0x00) = flags;
        }
    }
}
