typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;
typedef float f32;
typedef struct RuntimeUnit RuntimeUnit;

#define UNIT_FIELD(type, offset) (*(type *)((u8 *)unit + (offset)))
#define RECORD_FIELD(record, type, offset) (*(type *)((u8 *)(record) + (offset)))
extern RuntimeUnit *D_801F0CB0[];
extern f32 D_801F3A38;

s32 func_00128AD0(RuntimeUnit *unit)
{
    f32 position[3];
    f32 minimum;
    f32 radius;
    s32 best;
    s32 *cursor;
    s32 index;

    if (UNIT_FIELD(s32 *, 0xA8) == 0)
        return -1;

    minimum = 4050.0f;
    radius = D_801F3A38 * 1.8f;
    best = -1;
    if (UNIT_FIELD(u32, 0x00) & 0x2000) {
        position[0] = UNIT_FIELD(f32, 0x28);
        position[1] = UNIT_FIELD(f32, 0x2C);
        position[2] = UNIT_FIELD(f32, 0x30);
    } else {
        position[0] = UNIT_FIELD(f32, 0x08);
        position[1] = UNIT_FIELD(f32, 0x0C);
        position[2] = UNIT_FIELD(f32, 0x10);
    }

    cursor = UNIT_FIELD(s32 *, 0xA8);
    for (index = *cursor; index != -1; index = *cursor) {
        RuntimeUnit *target = D_801F0CB0[index];

        if ((RECORD_FIELD(target, u32, 0x00) & 0x11) == 0x11) {
            f32 dx = position[0] - RECORD_FIELD(target, f32, 0x08);
            f32 dz = position[2] - RECORD_FIELD(target, f32, 0x10);
            f32 distance = dx * dx + dz * dz;

            if (distance <= minimum) {
                minimum = distance;
                best = index;
            }
        }
        cursor++;
    }
    if (minimum < radius * radius)
        return best;
    return -1;
}
