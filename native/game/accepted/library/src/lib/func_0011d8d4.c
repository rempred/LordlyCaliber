#include "common/types.h"
typedef float f32;
typedef struct Q7Unit Q7Unit;
#define FIELD(p, type, offset) (*(type *)((u8 *)(p) + (offset)))
extern s32 D_801F3658;
extern Q7Unit *D_801F0CB0[];
extern u8 D_80214A63;
extern u8 D_801F365D;
extern s32 D_801F0DE8, D_801F0DE0, D_801F1074;
extern u8 D_80210219;
extern void func_00109C04(void);
extern void func_001072B8(Q7Unit *);

static __inline__ u32 clear_flag(u32 value, u32 flag)
{
    return value & ~flag;
}

void func_0011D8D4(s32 arg0)
{
    Q7Unit *unit = D_801F0CB0[D_801F3658];
    u32 previous;
    u32 flags;
    f32 x, y, z;
    s32 word_work;
    /* Preserve the observed 48-byte frame without adding instructions. */
    volatile u8 frame_pad[16];
    u32 first_flags;
    u32 state;
    s32 word74;
    f32 negative;
    s32 reset_value;
    u8 byte98;

    func_00109C04();
    func_001072B8(unit);
    first_flags = FIELD(unit, u32, 0);
    /* The real intervening byte and predicate phases keep the two sentinel
     * materializations separate in KMC GCC 2.7.2. */
    reset_value = -1;
    FIELD(unit, s32, 0x84) = reset_value;
    if (first_flags & 0x2000) {
        /* Reuse the later word-copy temporary through the mask constant and
         * its result; separate temporaries change the first register choices. */
        word_work = ~0x2000;
        word_work &= first_flags;
        /* These ordered accesses retain clear-before-byte-copy scheduling. */
        FIELD(unit, volatile u32, 0) = word_work;
        byte98 = FIELD(unit, volatile u8, 0x98);
        FIELD(unit, u8, 0x98) = 0;
        FIELD(unit, s32, 0x84) = reset_value;
        reset_value = byte98;
        FIELD(unit, u8, 0x91) = (u8)reset_value;
        FIELD(unit, f32, 0x9C) = -1.0f;
    }
    reset_value = arg0;
    if (reset_value != 0)
        FIELD(unit, u8, 0x92) = D_80214A63;
    state = D_801F365D;
    negative = -1.0f;
    D_801F0DE8 = 0;
    D_801F0DE0 = 0;
    previous = FIELD(unit, u32, 0);
    word74 = FIELD(unit, s32, 0x74);
    /* The real intervening byte and predicate phases keep the two sentinel
     * materializations separate in KMC GCC 2.7.2. */
    reset_value = -1;
    D_801F3658 = reset_value;
    D_801F1074 = reset_value;
    FIELD(unit, s32, 0x6C) = 0;
    D_801F365D = state & 0xFE;
    flags = clear_flag(clear_flag(clear_flag(previous, 2), 4), 0x200);
    FIELD(unit, u8, 0x20) = 0;
    FIELD(unit, u8, 0x98) = 0;
    FIELD(unit, f32, 0x9C) = negative;
    FIELD(unit, u32, 0) = flags;
    if (word74 != reset_value && FIELD(unit, u8, 0x92) == 0 &&
        (!(previous & 0x40) || FIELD(unit, u8, 0x91) == 0)) {
        /* Keep the three float reads before the word read. With ordinary
         * reads GCC hoists the reused word temporary ahead of them. */
        x = FIELD(unit, volatile f32, 0x08);
        y = FIELD(unit, volatile f32, 0x0C);
        z = FIELD(unit, volatile f32, 0x10);
        word_work = FIELD(unit, volatile s32, 0x14);
        FIELD(unit, u32, 0) = flags | 0x800000;
        FIELD(unit, f32, 0x4C) = x;
        FIELD(unit, f32, 0x50) = y;
        FIELD(unit, f32, 0x54) = z;
        FIELD(unit, s32, 0x64) = word_work;
    }
    D_80210219 = 0x11;
}



