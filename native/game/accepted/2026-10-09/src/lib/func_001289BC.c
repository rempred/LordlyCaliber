typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
extern s32 D_801EACD0;
extern u8 D_800E7AB9;
extern f32 func_00128050(RuntimeUnit *unit);
extern void func_00106CE0(f32 *field18, f32 *field94, f32 step);
extern s32 func_001284C4(RuntimeUnit *unit, f32 *output08, f32 *output10);
extern f32 func_000F3428(u8 selector, f32 field08, f32 field10);
extern f32 func_000F315C(u8 selector, f32 field08, f32 field10);

void func_001289BC(RuntimeUnit *unit)
{
    f32 output08;
    f32 output10;
    f32 query = func_00128050(unit);
    s32 next;

    /* Branch-local counter publications give the pinned compiler the retail
       store order before the helper argument setup. Unsigned subtraction
       preserves the observed 32-bit decrement wrap. */
    if (query != -1.0f) {
        next = D_801EACD0;
        if (next < 0) {
            UNIT_FIELD(f32, 0x94) = query;
            D_801EACD0 = 10;
        } else {
            D_801EACD0 = (s32)((u32)next - 1);
        }
    } else {
        D_801EACD0 = (s32)((u32)D_801EACD0 - 1);
    }
    func_00106CE0(&UNIT_FIELD(f32, 0x18), &UNIT_FIELD(f32, 0x94), 1.0f);
    UNIT_FIELD(f32, 0x1C) = UNIT_FIELD(f32, 0x18);
    if (func_001284C4(unit, &output08, &output10) != 0) {
        /* Capture both returned values before publishing either field. */
        f32 field08 = output08;
        f32 field10 = output10;
        s32 field70 = UNIT_FIELD(s32, 0x70);
        UNIT_FIELD(f32, 0x08) = field08;
        UNIT_FIELD(f32, 0x10) = field10;
        if (field70 == 1) {
            UNIT_FIELD(f32, 0x0C) = func_000F3428(D_800E7AB9, field08, field10);
        } else {
            UNIT_FIELD(f32, 0x0C) = func_000F315C(D_800E7AB9, output08, output10);
        }
    }
    if ((UNIT_FIELD(u8, 0xB8) & 0xF0) != 0x30) {
        UNIT_FIELD(u8, 0xB8) = 0x30;
    }
    UNIT_FIELD(u8, 0xB9) = 2;
}
