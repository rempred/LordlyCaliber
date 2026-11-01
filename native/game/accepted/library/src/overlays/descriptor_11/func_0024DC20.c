#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"
#include "game/effect_dispatch_e07.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

typedef struct P04Vec3 {
    float x, y, z;
} P04Vec3;

typedef struct Dc20InputRecord {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 flags_04;
} Dc20InputRecord;

typedef struct Dc20Header {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    P04Vec3 value_1C;
} Dc20Header;


extern void *p01_table_8020A170[];
extern P04Vec3 D_8020A230;
extern float D_80220EE0[];

void *func_0024DC20(u8 value, Dc20InputRecord *record, s32 index)
{
    P01Vec3 position;
    P04Vec3 basis;
    Dc20Header *object;
    P01Vec3 *vector;
    u16 flags;
    s16 selected;
    u8 attributes;
    P04Vec3 *payload;

    func_0024BF04(value, &position, record->value_02 & 0xF0);
    object = (Dc20Header *)func_002698A0(record->value_00, p01_table_8020A170[index]);
    flags = record->flags_04;
    object->flags_0A |= flags & 0xD000;
    vector = &position;
    if (flags & 0x1000) {
        if (flags & 0x400) {
            selected = (flags & 0x800) ? 10000 : -10000;
        } else {
            selected = (flags & 0x800) ? 1000 : -1000;
        }
        object->value_0C = selected;
    }
    attributes = record->value_02;
    basis = D_8020A230;
    payload = &object->value_1C;
    if (attributes & 0x40) {
        func_0025F058(D_80220EE0, payload);
    } else {
        if (attributes & 0x80) {
            basis.x = -vector->x < 0.0f ? -1.0f : 1.0f;
        }
        func_0026C230(&basis, payload);
    }
    func_0025F150(payload, vector);
    return (u8 *)payload - 0x1C;
}
