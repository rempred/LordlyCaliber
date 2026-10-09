typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;

typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))

extern f32 D_801F0D98;
extern f32 D_801F0D9C;
extern f32 D_801F0DA0;
extern f32 D_801F0DA4;
extern void func_001072B8(RuntimeUnit *unit);

void func_00121724(RuntimeUnit *unit, f32 arg1, f32 arg2, f32 arg3)
{
    u32 flags;
    func_001072B8(unit);
    /* Byte views preserve this compiler's flag-read and reset-store order.
       A disjoint-field struct lets it move the resets past the float work. */
    flags = UNIT_FIELD(u32, 0x00);
    UNIT_FIELD(u8, 0x91) = 0;
    UNIT_FIELD(s32, 0x84) = -1;
    UNIT_FIELD(s32, 0x80) = -1;
    UNIT_FIELD(s32, 0x88) = 0;
    UNIT_FIELD(u8, 0x92) = 0;
    UNIT_FIELD(f32, 0x28) = arg1;
    UNIT_FIELD(f32, 0x2C) = arg2;
    UNIT_FIELD(f32, 0x30) = arg3;
    UNIT_FIELD(u32, 0x00) = flags & ~2;
    /* Destructive float updates retain the original saved-register lifetimes. */
    arg3 -= D_801F0D9C;
    arg3 *= 64.0f;
    arg3 /= D_801F0DA4 - D_801F0D9C;
    arg1 -= D_801F0D98;
    arg1 *= 64.0f;
    arg1 /= D_801F0DA0 - D_801F0D98;
    UNIT_FIELD(u8, 0x20) = 1;
    UNIT_FIELD(s32, 0x24) = 1;
    UNIT_FIELD(u8, 0x91) = 2;
    UNIT_FIELD(s32, 0x88) = 90;
    UNIT_FIELD(u32, 0x00) &= ~4;
    UNIT_FIELD(u32, 0x00) &= ~0x200;
    UNIT_FIELD(u32, 0x00) |= 2;
    UNIT_FIELD(u32, 0x00) &= ~0x00800000;
    UNIT_FIELD(s32, 0x84) = -1;
    UNIT_FIELD(s32, 0x58) = ((s32)arg3 << 6) + (s32)arg1;
}
