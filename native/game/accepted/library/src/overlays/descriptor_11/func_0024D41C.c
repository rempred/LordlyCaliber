#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"

typedef struct P04Vec3 {
    float x, y, z;
} P04Vec3;

typedef struct D41cInputRecord {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 flags_04;
} D41cInputRecord;

typedef struct D41cHeader {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
    P04Vec3 value_1C;
} D41cHeader;

typedef void *(*D41cCallback)(s32 argument);
extern D41cCallback g_func_0024D41C_callbacks[];
extern P04Vec3 D_8020A230;
extern float D_80220EE0[];

void *func_0024D41C(void *vectorView, void *recordView)
{
    P04Vec3 *vector = (P04Vec3 *)vectorView;
    D41cInputRecord *record = (D41cInputRecord *)recordView;
    s32 mode = record->value_02 & 3;
    P04Vec3 basis;

    switch (mode) {
    case 0: {
        D41cHeader *object;
        u16 flags;
        s16 selected;

        object = g_func_0024D41C_callbacks[mode](record->value_00);
        flags = record->flags_04;
        object->flags_0A |= flags & 0xD000;
        if (flags & 0x1000) {
            if (flags & 0x400) {
                selected = (flags & 0x800) ? 10000 : -10000;
            } else {
                selected = (flags & 0x800) ? 1000 : -1000;
            }
            object->value_0C = selected;
        }
        object->value_1C.x = vector->x;
        object->value_1C.y = vector->y;
        object->value_1C.z = vector->z;
        return object;
    }
    case 1: {
        D41cHeader *object;
        u16 flags;
        s16 selected;
        u8 attributes;
        P04Vec3 *payload;

        object = g_func_0024D41C_callbacks[mode](record->value_00);
        flags = record->flags_04;
        object->flags_0A |= flags & 0xD000;
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
    case 2: {
        D41cHeader *object;
        u16 flags;
        s16 selected;
        u8 attributes;
        P04Vec3 *payload;

        object = g_func_0024D41C_callbacks[mode](record->value_00);
        flags = record->flags_04;
        object->flags_0A |= flags & 0xD000;
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
    }
    return 0;
}
