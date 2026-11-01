#include "game/effect_adapters_e08.h"
#include "game/effect_math_e04.h"

typedef struct P01Vec3 {
    float x, y, z;
} P01Vec3;

typedef struct P04Vec3 {
    float x, y, z;
} P04Vec3;

typedef struct DfccInputRecord {
    u16 value_00;
    u8 value_02;
    u8 unknown_03;
    u16 flags_04;
} DfccInputRecord;

typedef struct DfccHeader {
    u8 unknown_00[0xA];
    u16 flags_0A;
    s32 value_0C;
    u8 unknown_10[0xC];
} DfccHeader;

typedef struct DfccPositionEntry {
    P01Vec3 position;
    u8 unknown_0C[0xC];
} DfccPositionEntry;

typedef struct DfccPositionPayload {
    DfccPositionEntry *entries;
} DfccPositionPayload;

typedef float DfccMatrix[4][4];

typedef struct DfccMatrixPayload {
    DfccMatrix *entries;
} DfccMatrixPayload;

typedef void *(*DfccFactory)(u32 value, u32 count);
extern DfccFactory g_func_0024DFCC_callbacks[];
extern P04Vec3 D_8020A230;
extern float D_80220EE0[];

void *func_0024DFCC(P01Vec3 *vectors, void *recordView, s32 count)
{
    DfccInputRecord *record = (DfccInputRecord *)recordView;
    s32 mode = record->value_02 & 3;
    P04Vec3 basis;
    s16 i;

    switch (mode) {
    case 0: {
        DfccHeader *object;
        DfccPositionPayload *payload;
        u16 flags;
        s16 selected;

        object = g_func_0024DFCC_callbacks[mode](record->value_00, (u8)count);
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
        payload = (DfccPositionPayload *)((u8 *)object + 0x1C);
        for (i = 0; i < (u8)count; i++) {
            payload->entries[i].position.x = vectors[i].x;
            payload->entries[i].position.y = vectors[i].y;
            payload->entries[i].position.z = vectors[i].z;
        }
        return (u8 *)payload - 0x1C;
    }
    case 1: {
        DfccHeader *object;
        DfccMatrixPayload *payload;
        u16 flags;
        s16 selected;

        object = g_func_0024DFCC_callbacks[mode](record->value_00, (u8)count);
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
        payload = (DfccMatrixPayload *)((u8 *)object + 0x1C);
        for (i = 0; i < (u8)count; i++) {
            u8 attributes;
            P01Vec3 *vector;
            DfccMatrix *matrix;
            DfccMatrix *entries;

            attributes = record->value_02;
            entries = payload->entries;
            basis = D_8020A230;
            /* Keep retail offset-first sums; pointer indexing reverses them. */
            vector = (P01Vec3 *)((u32)(i * sizeof(*vectors)) + (u32)vectors);
            matrix = (DfccMatrix *)((u32)(i * sizeof(*entries)) + (u32)entries);
            if (attributes & 0x40) {
                func_0025F058(D_80220EE0, matrix);
            } else {
                if (attributes & 0x80) {
                    basis.x = -vector->x < 0.0f ? -1.0f : 1.0f;
                }
                func_0026C230(&basis, matrix);
            }
            func_0025F150(matrix, vector);
        }
        return (u8 *)payload - 0x1C;
    }
    case 2: {
        DfccHeader *object;
        DfccMatrixPayload *payload;
        u16 flags;
        s16 selected;

        object = g_func_0024DFCC_callbacks[mode](record->value_00, (u8)count);
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
        payload = (DfccMatrixPayload *)((u8 *)object + 0x1C);
        for (i = 0; i < (u8)count; i++) {
            u8 attributes;
            P01Vec3 *vector;
            DfccMatrix *matrix;
            DfccMatrix *entries;

            attributes = record->value_02;
            entries = payload->entries;
            basis = D_8020A230;
            /* Keep retail offset-first sums; pointer indexing reverses them. */
            vector = (P01Vec3 *)((u32)(i * sizeof(*vectors)) + (u32)vectors);
            matrix = (DfccMatrix *)((u32)(i * sizeof(*entries)) + (u32)entries);
            if (attributes & 0x40) {
                func_0025F058(D_80220EE0, matrix);
            } else {
                if (attributes & 0x80) {
                    basis.x = -vector->x < 0.0f ? -1.0f : 1.0f;
                }
                func_0026C230(&basis, matrix);
            }
            func_0025F150(matrix, vector);
        }
        return (u8 *)payload - 0x1C;
    }
    }
    return 0;
}
