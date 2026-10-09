typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct { u8 bytes[18]; } WakeRecord18;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))

extern u8 D_801971F1[][25];
extern WakeRecord18 D_801F0E76[];
extern void func_0010746C(RuntimeUnit *unit);
extern void func_001072B8(RuntimeUnit *unit);
extern void func_00121F38(RuntimeUnit *unit, WakeRecord18 *record);

void func_00121DA8(RuntimeUnit *unit)
{
    u32 flags = UNIT_FIELD(u32, 0x00);
    s32 state;
    u32 reset_flags;
    u32 stop_flags;
    f32 field08, field0C, field10;
    s32 field14;

    if (flags & 0x10) {
        UNIT_FIELD(u32, 0x00) = flags | 1;
        D_801971F1[UNIT_FIELD(u8, 0x04)][0] |= 4;
        func_0010746C(unit);
        reset_flags = UNIT_FIELD(u32, 0x00);
        field08 = UNIT_FIELD(f32, 0x08);
        field0C = UNIT_FIELD(f32, 0x0C);
        field10 = UNIT_FIELD(f32, 0x10);
        field14 = UNIT_FIELD(s32, 0x14);
        state = UNIT_FIELD(u8, 0xBA);
        UNIT_FIELD(s32, 0x88) = 0;
        UNIT_FIELD(u8, 0x92) = 0;
        UNIT_FIELD(u8, 0x91) = 0;
        UNIT_FIELD(s32, 0x80) = -1;
        UNIT_FIELD(s32, 0x84) = -1;
        reset_flags &= ~0x00800000;
        reset_flags &= ~0x2000;
        UNIT_FIELD(u32, 0x00) = reset_flags;
        UNIT_FIELD(f32, 0x4C) = field08;
        UNIT_FIELD(f32, 0x50) = field0C;
        UNIT_FIELD(f32, 0x54) = field10;
        UNIT_FIELD(s32, 0x64) = field14;
        switch (state) {
        case 0:
            break;
        case 1:
            break;
        case 2:
            UNIT_FIELD(u32, 0x00) |= 0x00800000;
            break;
        case 3:
            func_001072B8(unit);
            UNIT_FIELD(u32, 0x00) &= ~2;
            /* Preserve the second retail flag read after the preceding mask store.
               This is a compiler scheduling requirement, not an asynchronous-state claim. */
            stop_flags = *(volatile u32 *)unit;
            UNIT_FIELD(u8, 0x91) = 0;
            UNIT_FIELD(s32, 0x84) = -1;
            UNIT_FIELD(s32, 0x88) = 0;
            UNIT_FIELD(u8, 0x92) = 1;
            UNIT_FIELD(f32, 0x9C) = -1.0f;
            stop_flags &= ~0x00800000;
            UNIT_FIELD(u32, 0x00) = stop_flags;
            break;
        default:
            func_00121F38(unit, &D_801F0E76[UNIT_FIELD(u8, 0xBA)]);
            break;
        }
    }
}
