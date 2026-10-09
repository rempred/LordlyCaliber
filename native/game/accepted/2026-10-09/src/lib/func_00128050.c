typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define RECORD_FIELD(record, type, offset) (*(type *)((u8 *)(record) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern f32 D_801F3A38;
extern f64 D_801EE808;
extern f64 D_801EE810;
extern f64 D_801EE818;
extern s32 func_801F6B64(RuntimeUnit *target);
extern f32 func_8009CFE0(f32 first, f32 second);

f32 func_00128050(RuntimeUnit *unit)
{
    f32 result = -1.0f;
    s32 count;
    s32 index;
    s32 *list = UNIT_FIELD(s32 *, 0xA8);

    if (list != 0) {
        count = 0;
        if (*list != -1) {
            f64 half = D_801EE808;

            for (index = 0; UNIT_FIELD(s32 *, 0xA8)[index] != -1; index++) {
                RuntimeUnit *target = D_801F0CB0[UNIT_FIELD(s32 *, 0xA8)[index]];

                if ((RECORD_FIELD(target, u32, 0x00) & 0x11) == 0x11 &&
                    func_801F6B64(target) >= 200) {
                    f32 dx = RECORD_FIELD(target, f32, 0x08) - UNIT_FIELD(f32, 0x08);
                    f32 dz = -(RECORD_FIELD(target, f32, 0x10) - UNIT_FIELD(f32, 0x10));
                    f32 radius;
                    f32 angle;

                    if (!(UNIT_FIELD(u32, 0x00) & 8))
                        radius = D_801F3A38 * 1.5f;
                    else
                        radius = D_801F3A38 * 1.8f;

                    /* Preserve the two observed additions of dz. The retail
                       filter does not square this operand. */
                    if (!(radius * radius < (dx * dx + dz) + dz)) {
                        if (dx == 0.0f) {
                            if (dz > 0.0f)
                                angle = 0.25f;
                            else if (dz < 0.0f)
                                angle = 0.75f;
                            else
                                angle = UNIT_FIELD(f32, 0x18);
                        } else if (dz == 0.0f) {
                            if (dx > 0.0f)
                                angle = 0.0f;
                            else if (dx < 0.0f)
                                angle = 0.5f;
                            else
                                angle = UNIT_FIELD(f32, 0x18);
                        } else {
                            angle = (f32)((f64)func_8009CFE0(dz, dx) / D_801EE810);
                        }
                        angle = (f32)((f64)angle + half);
                        while (angle < 0.0f)
                            angle += 1.0f;
                        while (angle >= 1.0f)
                            angle -= 1.0f;

                        if (result == -1.0f) {
                            result = angle;
                        } else {
                            f32 difference = result - angle;
                            f32 mean;

                            if (!(difference > 0.0f))
                                difference = -difference;
                            if ((f64)difference <= half)
                                mean = (result + angle) / 2.0f;
                            else
                                mean = (result + angle + 1.0f) / 2.0f;
                            while (mean < 0.0f)
                                mean += 1.0f;
                            while (mean >= 1.0f)
                                mean -= 1.0f;
                            result = mean;
                        }
                    }
                }
                count++;
            }
        }
        if ((count == 0) | (result == -1.0f))
            return -1.0f;

        {
            f32 prior = UNIT_FIELD(f32, 0x18);
            f32 difference = result - prior;
            f32 mean;

            if (!(difference > 0.0f))
                difference = -difference;
            if ((f64)difference <= D_801EE818)
                mean = (result + prior) / 2.0f;
            else
                mean = (result + prior + 1.0f) / 2.0f;
            while (mean < 0.0f)
                mean += 1.0f;
            while (mean >= 1.0f)
                mean -= 1.0f;
            return mean;
        }
    }
    return result;
}
