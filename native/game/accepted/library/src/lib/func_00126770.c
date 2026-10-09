typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern u8 D_801969B8[];
extern RuntimeUnit *D_801F0CB0[];
extern s32 func_0012E968(RuntimeUnit *unit);
extern s32 func_0012E9F4(RuntimeUnit *unit);
extern s32 func_0012E950(s32 index);

/* Keep the original eleven-byte group stride and unchecked row/member
 * results. The native switch table covers selectors 1 through 8.
 *
 * The working floats are shared across cases to preserve GCC 2.7 allocation.
 * The first three cases keep a named zero comparison boundary; case 4 has
 * its own leader value. Once the heading is formed, base/difference hold
 * the actual zero boundary and unit increment for lower normalization. */
f32 func_00126770(RuntimeUnit *first, RuntimeUnit *unit)
{
    f32 base, leaderHeading, difference, result, normalizationFloor;
    s32 row = func_0012E968(first);
    s32 member = func_0012E9F4(first);
    u8 *group = D_801969B8 + row * 11;
    RuntimeUnit *leader = D_801F0CB0[func_0012E950(group[2])];
    if (FIELD(leader, u32, 0) & 0xC000)
        return FIELD(unit, f32, 0xAC);
    switch (group[8]) {
    case 1:
    case 7:
    case 8:
    {
        if (FIELD(unit, u32, 0) & 2)
            base = FIELD(unit, f32, 0x18);
        else
            base = FIELD(leader, f32, 0x18) - 0.5f;
        normalizationFloor = 0.0f;
        if (base < normalizationFloor)
            base += 1.0f;
        leaderHeading = FIELD(leader, f32, 0x18);
        difference = base - leaderHeading;
        if (difference < normalizationFloor)
            difference += 1.0f;
        difference -= 0.5f;
        if (difference < normalizationFloor)
            difference += 1.0f;
        base = 1.0f;
        result = base - difference + leaderHeading;
        if (result < normalizationFloor) {
            difference = 1.0f;
            base = normalizationFloor;
            do {
                result += difference;
            } while (result < base);
        }
        while (1.0f <= result)
            result -= 1.0f;
        return result;
    }
    case 2:
minusEighth:
    {
        if (FIELD(unit, u32, 0) & 2)
            base = FIELD(unit, f32, 0x18);
        else
            base = FIELD(leader, f32, 0x18) - 0.5f;
        normalizationFloor = 0.0f;
        if (base < normalizationFloor)
            base += 1.0f;
        leaderHeading = FIELD(leader, f32, 0x18);
        difference = base - leaderHeading;
        if (difference < normalizationFloor)
            difference += 1.0f;
        difference -= 0.5f;
        if (difference < normalizationFloor)
            difference += 1.0f;
        base = 0.125f;
        base = difference - base;
        result = 1.125f - base + leaderHeading;
        if (result < normalizationFloor) {
            difference = 1.0f;
            base = normalizationFloor;
            do {
                result += difference;
            } while (result < base);
        }
        while (1.0f <= result)
            result -= 1.0f;
        return result;
    }
    case 3:
plusEighth:
    {
        if (FIELD(unit, u32, 0) & 2)
            base = FIELD(unit, f32, 0x18);
        else
            base = FIELD(leader, f32, 0x18) - 0.5f;
        normalizationFloor = 0.0f;
        if (base < normalizationFloor)
            base += 1.0f;
        leaderHeading = FIELD(leader, f32, 0x18);
        difference = base - leaderHeading;
        if (difference < normalizationFloor)
            difference += 1.0f;
        difference -= 0.5f;
        if (difference < normalizationFloor)
            difference += 1.0f;
        base = 0.125f;
        base = difference + base;
        result = 0.875f - base + leaderHeading;
        if (result < normalizationFloor) {
            difference = 1.0f;
            base = normalizationFloor;
            do {
                result += difference;
            } while (result < base);
        }
        while (1.0f <= result)
            result -= 1.0f;
        return result;
    }
    case 4:
    {
        f32 quarterLeaderHeading;
        if (FIELD(unit, u32, 0) & 2)
            base = FIELD(unit, f32, 0x18);
        else
            base = FIELD(leader, f32, 0x18) - 0.25f;
        if (base < 0.0f)
            base += 1.0f;
        quarterLeaderHeading = FIELD(leader, f32, 0x18);
        difference = base - quarterLeaderHeading;
        if (difference < 0.0f)
            difference += 1.0f;
        base = 1.0f;
        result = base - difference + quarterLeaderHeading;
        if (result < 0.0f) {
            difference = 1.0f;
            base = 0.0f;
            do {
                result += difference;
            } while (result < base);
        }
        while (1.0f <= result)
            result -= 1.0f;
        return result;
    }
    case 5:
    case 6:
        if ((member == 1) | (member == 3))
            goto minusEighth;
        goto plusEighth;
    default:
        if ((member == 1) | (member == 3))
            goto plusEighth;
        goto minusEighth;
    }
}
