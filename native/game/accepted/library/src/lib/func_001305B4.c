typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern u8 D_801971F2[];
extern u8 D_80193BD8[], D_80195578[];
extern void func_0010746C(RuntimeUnit *unit);

/* Numeric views preserve the observed 25/56/52-byte strides and the two
 * independent descriptor queries. They establish no array capacity. */
static inline s32 descriptor_value_present(RuntimeUnit *unit)
{
    u32 id = FIELD(unit, u8, 0x04);
    s32 value = D_801971F2[id * 25];
    s32 result = 0;
    if ((value == 0) | (value >= 100))
        return 0;
    {
        if (id < 30)
            result = FIELD(D_80193BD8 + value * 56, u16, 0) != 0;
        else
            result = FIELD(D_80195578 + value * 52, u16, 0) != 0;
    }
    return result;
}

static inline s32 descriptor_flag_present(RuntimeUnit *unit)
{
    u32 id = FIELD(unit, u8, 0x04);
    s32 value = D_801971F2[id * 25];
    s32 result = 0;
    if ((value == 0) | (value >= 100))
        return 0;
    {
        s32 flags;
        if (id < 30)
            flags = FIELD(D_80193BD8 + value * 56, u8, 0x1B) & 4;
        else
            flags = FIELD(D_80195578 + value * 52, u8, 0x1B) & 4;
        result = flags != 0;
    }
    return result;
}

static inline u32 clear_flags(u32 flags, u32 mask)
{
    return flags & ~mask;
}

s32 func_001305B4(RuntimeUnit *unit)
{
    s32 result = 0;
    if (FIELD(unit, u32, 0x00) & 0x40)
        return 0;
    if (!descriptor_value_present(unit) || descriptor_flag_present(unit)) {
        u32 flags, publishedFlags;
        /* This volatile view retains the two observed flag-word reads. Separate
         * values preserve their allocation; this is a compiler workaround,
         * without evidence of original qualifiers or concurrent access. */
        volatile u32 *flagWord = (volatile u32 *)unit;
        f32 x, y, z;
        s32 index;
        func_0010746C(unit);
        result = 1;
        flags = *flagWord;
        x = FIELD(unit, f32, 0x08);
        y = FIELD(unit, f32, 0x0C);
        z = FIELD(unit, f32, 0x10);
        index = FIELD(unit, s32, 0x14);
        *flagWord = (flags & ~0x2000U) | 0x800000;
        publishedFlags = FIELD(unit, u32, 0x00);
        FIELD(unit, u8, 0x92) = 0;
        FIELD(unit, u8, 0x91) = 0;
        FIELD(unit, s32, 0x84) = -1;
        FIELD(unit, s32, 0x80) = -1;
        FIELD(unit, u8, 0x92) = 2;
        FIELD(unit, u8, 0xBA) = 1;
        FIELD(unit, f32, 0x4C) = x;
        FIELD(unit, f32, 0x50) = y;
        FIELD(unit, f32, 0x54) = z;
        FIELD(unit, s32, 0x64) = index;
        *flagWord = clear_flags(clear_flags(publishedFlags, 0x08000000), 0x800000);
    }
    return result;
}
