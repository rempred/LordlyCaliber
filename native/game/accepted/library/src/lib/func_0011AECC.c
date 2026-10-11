#include "common/types.h"
typedef float f32;
typedef double f64;
typedef struct RuntimeUnit RuntimeUnit;
typedef struct RouteRecord {
    f32 *field00, *field04, *field08, *field0C, *field10;
    s32 field14;
    f32 field18;
    f32 field1C;
} RouteRecord;
#define FIELD(object, type, offset) (*(type *)((u8 *)(object) + (offset)))
extern f32 D_801F0D98, D_801F0D9C, D_801F0DA0, D_801F0DA4;
extern char D_801EE3EC[];
extern void *resource_alloc(u32 bytes);
extern void func_00023940(const char *format, ...);
extern s32 func_0011ABD4(RuntimeUnit *unit, s32 index);
extern void func_0011A540(f32 *first, f32 *second, f32 *third, s32 count);
extern void func_0011A9B8(f32 value, f32 *firstOut, f32 *secondOut, f32 *third,
    f32 *first, f32 *second, f32 *fourth, f32 *fifth, s32 count);
extern f32 func_0002CB80(f32 first, f32 second);

void func_0011AECC(RuntimeUnit *unit)
{
    s32 *route = FIELD(unit, s32 *, 0x68);
    s32 count = 0;
    s32 i;
    s32 pointIndex;
    s32 phaseCount;
    RouteRecord *record;
    f32 lowX, lowZ, spanX, spanZ;
    f32 *first, *second, *third, *fourth, *fifth;
    f32 outFirst, outSecond;
    f32 *firstTarget;
    f32 *secondTarget;

    if (route[0] != -1) {
        do {
            ++count;
        } while (route[count] != -1);
    }
    if (count < 3)
        return;

    record = resource_alloc(0x20);
    if (record == 0)
        func_00023940(D_801EE3EC);
    lowX = D_801F0D98;
    lowZ = D_801F0D9C;
    spanX = D_801F0DA0 - lowX;
    spanZ = D_801F0DA4 - lowZ;
    record->field00 = resource_alloc(count * 4);
    record->field04 = resource_alloc(count * 4);
    record->field08 = resource_alloc(count * 4);
    record->field0C = resource_alloc(count * 4);
    record->field10 = resource_alloc(count * 4);
    record->field00[0] = FIELD(unit, f32, 0x08);
    record->field04[0] = FIELD(unit, f32, 0x10);

    pointIndex = 1;
    while (pointIndex < count) {
        /* The real route argument is consumed before the returned point. */
        s32 point = route[pointIndex];
        point = func_0011ABD4(unit, point);
        record->field00[pointIndex] = lowX + (f32)(point % 256) * spanX / 256.0f + spanX / 512.0f;
        record->field04[pointIndex] = lowZ + (f32)(point / 256) * spanZ / 256.0f + spanZ / 512.0f;
        ++pointIndex;
    }
    if (FIELD(unit, u8, 0x91) == 1 && FIELD(unit, s32, 0x84) != -1) {
        /* The existing source float is consumed before destination setup. */
        firstTarget = (f32 *)((u8 *)unit + 0x4C);
        record->field00[pointIndex - 1] = *firstTarget;
        record->field04[pointIndex - 1] = FIELD(unit, f32, 0x54);
    } else {
        RuntimeUnit *point;
        u32 firstAddress;
        firstTarget = record->field00;
        /* Form the real address separately, then advance its pointer. */
        firstAddress = (u32)pointIndex * 4 + (u32)firstTarget;
        firstTarget = (f32 *)firstAddress;
        point = (RuntimeUnit *)((u8 *)unit + (FIELD(unit, s32, 0x24) - 1) * 12);
        firstTarget[-1] = FIELD(point, f32, 0x28);
        {
            secondTarget = record->field04;
            point = (RuntimeUnit *)((u8 *)unit + (FIELD(unit, s32, 0x24) - 1) * 12);
            secondTarget[pointIndex - 1] = FIELD(point, f32, 0x30);
        }
    }

    {
        f32 *capturedSecond = record->field04;
        third = record->field08;
        first = record->field00;
        second = capturedSecond;
    }
    fourth = record->field0C;
    fifth = record->field10;
    record->field14 = count;
    phaseCount = count;
    third[0] = 0.0f;
    for (i = 1; i < phaseCount; ++i) {
        f32 dx = first[i] - first[i - 1];
        f32 dz = second[i] - second[i - 1];
        third[i] = (f64)third[i - 1] + __builtin_fsqrt((f64)(dx * dx + dz * dz));
    }
    for (i = 1; i < phaseCount; ++i) {
        /* Form this invariant address after loop entry. The explicit word
         * address keeps the pinned compiler's offset-first operand order. */
        secondTarget = (f32 *)((u32)phaseCount * 4 + (u32)third);
        third[i] /= secondTarget[-1];
    }
    func_0011A540(third, first, fourth, phaseCount);
    func_0011A540(third, second, fifth, phaseCount);
    {
        f32 value = 0.01f;
        record->field1C = 0.0f;
        func_0011A9B8(value, &outFirst, &outSecond, record->field08,
            record->field00, record->field04, record->field0C, record->field10, record->field14);
        value = func_0002CB80(outFirst - record->field00[0], outSecond - record->field04[0]);
        value = 0.01f / value;
        value = value * 45.0f / 3840.0f;
        record->field18 = value;
    }
    FIELD(unit, RouteRecord *, 0xA4) = record;
}
