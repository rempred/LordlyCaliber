#include "common/types.h"
#include "game/effect_presentation_p04.h"
#include "game/effect_math_e04.h"

typedef struct P04Vec3 {
    float x, y, z;
} P04Vec3;

typedef struct P04PresentationSpec {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 value_04;
} P04PresentationSpec;

typedef struct P04PresentationObject {
    u8 unknown_00[10];
    u16 value_0A;
    s32 value_0C;
    u8 unknown_10[12];
    P04Vec3 position;
} P04PresentationObject;

typedef P04PresentationObject *(*P04Factory)(u32, s32, void *, s32, void *);
extern P04Factory D_8020974C[];
extern P04Vec3 D_8020A230;
extern float D_80220EE0[];

P04PresentationObject *func_0024DD7C(P04Vec3 *vector, P04PresentationSpec *spec, void *argument)
{
    s32 mode = spec->value_02 & 3;
    switch ((u8)mode) {
    case 0: {
        P04PresentationObject *object;
        u16 flags;
        s16 value;
        object = D_8020974C[mode](spec->value_00, 8, argument, 8, argument);
        flags = spec->value_04;
        object->value_0A |= flags & 0xD000;
        if (flags & 0x1000) {
            if (flags & 0x400) {
                if (flags & 0x800) value = 10000;
                else value = -10000;
            } else {
                if (flags & 0x800) value = 1000;
                else value = -1000;
            }
            object->value_0C = value;
        }
        object->position.x = vector->x;
        object->position.y = vector->y;
        object->position.z = vector->z;
        return object;
    }
    case 2: {
        P04PresentationObject *object;
        u16 flags;
        s16 value;
        u8 attributes;
        P04Vec3 basis;
        P04Vec3 *position;
        object = D_8020974C[mode](spec->value_00, 8, argument, 8, argument);
        flags = spec->value_04;
        object->value_0A |= flags & 0xD000;
        if (flags & 0x1000) {
            if (flags & 0x400) {
                if (flags & 0x800) value = 10000;
                else value = -10000;
            } else {
                if (flags & 0x800) value = 1000;
                else value = -1000;
            }
            object->value_0C = value;
        }
        attributes = spec->value_02;
        basis = D_8020A230;
        position = &object->position;
        if (attributes & 0x40) {
            func_0025F058(D_80220EE0, position);
        } else {
            if (attributes & 0x80) {
                basis.x = -vector->x < 0.0f ? -1.0f : 1.0f;
            }
            func_0026C230(&basis, position);
        }
        func_0025F150(position, vector);
        return (P04PresentationObject *)((u8 *)position - 0x1C);
    }
    }
    return 0;
}


