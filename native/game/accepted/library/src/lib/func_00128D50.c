typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
extern f32 func_00128050(RuntimeUnit *unit);

void func_00128D50(RuntimeUnit *unit)
{
    if (func_00128050(unit) != -1.0f) {
        u32 flags = UNIT_FIELD(u32, 0x00);
        f32 field08 = UNIT_FIELD(f32, 0x08);
        f32 field0C = UNIT_FIELD(f32, 0x0C);
        f32 field10 = UNIT_FIELD(f32, 0x10);
        s32 field14 = UNIT_FIELD(s32, 0x14);
        u8 field91 = UNIT_FIELD(u8, 0x91);
        f32 field18 = UNIT_FIELD(f32, 0x18);

        /* Both +0x88 stores occur in retail, around the +0x20 byte clear. */
        UNIT_FIELD(s32, 0x88) = 0x5A;
        UNIT_FIELD(u8, 0x20) = 0;
        UNIT_FIELD(s32, 0x88) = 0x5A;
        UNIT_FIELD(u32, 0x00) = flags | 0x2000;
        UNIT_FIELD(f32, 0x28) = field08;
        UNIT_FIELD(f32, 0x2C) = field0C;
        UNIT_FIELD(f32, 0x30) = field10;
        UNIT_FIELD(s32, 0x58) = field14;
        UNIT_FIELD(u8, 0x98) = field91;
        UNIT_FIELD(f32, 0x9C) = field18;
    }
}
