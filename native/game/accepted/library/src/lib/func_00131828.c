typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct { f32 x, y, z; } Vec3f;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern s32 D_801F1070;
extern u8 D_8018F481;
extern u8 D_801951CC[];
extern f32 D_801F3A38;
extern f64 D_801EEA38;
extern void func_0012EA80(s32 index, Vec3f *point);
static s32 func_00131984(void);

void func_00131828(RuntimeUnit *unit)
{
    if (FIELD(unit, u32, 0x78) & 1) {
        s32 index = 0;
        /* The actual initial and repeated guards use the same loop index. */
        if (index < D_801F1070) {
            f64 scale = D_801EEA38;
            u32 clearMask = ~1U;
            s32 recordOffset = 0;
            do {
                if (!func_00131984() &&
                    !(FIELD(D_801951CC + recordOffset, u16, 0) & 2)) {
                    Vec3f point;
                    f32 dx, dz;
                    f64 range;
                    func_0012EA80(index, &point);
                    range = (f64)D_801F3A38 * scale;
                    dx = FIELD(unit, f32, 0x08) - point.x;
                    dz = FIELD(unit, f32, 0x10) - point.z;
                    if ((f64)(dx * dx + dz * dz) < range * range) {
                        FIELD(unit, s32, 0x7C) = index;
                        FIELD(unit, u32, 0x78) =
                            (FIELD(unit, u32, 0x78) & clearMask) | 0x40;
                        return;
                    }
                }
                index++;
                recordOffset += 36;
            } while (index < D_801F1070);
        }
    }
}

/* These three numeric switches reproduce the complete observed dispatch
 * domains. They do not establish names for the global selector or its cases. */
static s32 func_00131948(void)
{
    s32 result = 0;
    switch (D_8018F481) {
    case 26: case 27: case 28: case 29:
    case 42: case 43: case 44:
    case 57: case 58:
        result = 1;
        break;
    case 30: case 31: case 32: case 33: case 34: case 35:
    case 36: case 37: case 38: case 39: case 40: case 41:
    case 45: case 46: case 47: case 48: case 49: case 50:
    case 51: case 52: case 53: case 54: case 55: case 56:
    default:
        break;
    }
    return result;
}

static s32 func_00131984(void)
{
    s32 result = 0;
    switch (D_8018F481) {
    case 39: case 47: case 48: case 51: case 60: case 61:
        result = 1;
        break;
    case 40: case 41: case 42: case 43: case 44: case 45: case 46:
    case 49: case 50: case 52: case 53: case 54: case 55:
    case 56: case 57: case 58: case 59:
    default:
        break;
    }
    return result;
}

static s32 func_001319C0(u32 selector)
{
    s32 result = 0;
    switch (selector) {
    case 0: case 2: case 8: case 16: case 17: case 18: case 19: case 23:
        result = 1;
        break;
    case 1: case 3: case 4: case 5: case 6: case 7: case 9:
    case 10: case 11: case 12: case 13: case 14: case 15:
    case 20: case 21: case 22:
    default:
        break;
    }
    return result;
}

static s32 func_001319F0(RuntimeUnit *unit)
{
    s32 result = 0;
    u32 input = FIELD(unit, u32, 0x00);
    input &= 8;
    if (input != 0) {
        input = FIELD(unit, u8, 0x04);
        /* These are the observed unsigned comparison and inversion phases. */
        result = (u8)input < 30;
        result ^= 1;
    }
    return result;
}
