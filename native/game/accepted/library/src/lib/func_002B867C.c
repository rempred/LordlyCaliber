#include "common/types.h"

typedef struct BriefingRow {
    void *components[3];
    u8 pad0C[0x2A];
    u16 field36;
    u16 field38;
    u16 field3A;
    u16 field3C;
    u8 pad3E[0x0A];
    s32 field48;
    s32 field4C;
    u8 pad50[8];
    s32 field58;
    u8 pad5C[0x9C];
} BriefingRow;

typedef union AnimationSeedWord {
    u32 bits;
    float copyView;
} AnimationSeedWord;

typedef struct BriefingActor {
    u8 pad000[0xE4];
    s32 slot;
    s32 field0E8;
    s32 field0EC;
    s32 field0F0;
    s32 sequenceTimer;
    s32 sequenceFlags;
    u8 pad0FC[0x20];
    float x;
    float y;
    float z;
    u8 pad128[0x0C];
    u16 state;
    u8 pad136[2];
    u16 animation;
    u16 field13A;
    u8 pad13C;
    u8 active;
    u8 pad13E[7];
    u8 field145;
    u8 field146;
    u8 rowIndex;
    u8 pad148;
    u8 sourceIndex;
    u8 pad14A[6];
} BriefingActor;

typedef struct BriefingScene {
    u8 pad00[0x18];
    BriefingActor *actors[28];
} BriefingScene;

extern BriefingScene *D_8022A974;
extern s32 func_0020C120(void *row);
extern void *resource_alloc(u32 size);
extern void func_00023780(void *record, u32 size);
extern s32 func_00045934(s32, s32, s32, s32);
extern void func_00207658(s32 *, s32 *, s32 *, s32 *, s32 *, u32);
extern void func_8022E9E8(void *actor);

void func_002B867C(BriefingRow *row, s32 rowIndex, s32 mode, s32 field48,
                  s32 field4C, u8 anchor, s16 initialAnimation)
{
    s32 slots[3];
    s32 tag;
    u32 anchorSlot;
    u32 anchorOffset;
    AnimationSeedWord seed;
    AnimationSeedWord animationWord;
    u16 animation;
    s32 cloneSlot = 0;
    BriefingActor *actor = 0;
    BriefingActor *previous = 0;
    s32 ordinal;

    seed.bits = (u16)initialAnimation;
    if (func_0020C120(row) == 1) {
        BriefingScene *scene = D_8022A974;

        for (cloneSlot = 0; cloneSlot < 28; cloneSlot++) {
            if (scene->actors[cloneSlot] == 0) {
                break;
            }
        }
    }

    ordinal = 0;
    anchorSlot = (u8)anchor;
    anchorOffset = anchorSlot * sizeof(BriefingActor *);
    /* Preserve the seed bits while giving KMC the retail setup order. */
    animationWord.copyView = seed.copyView;
    animation = (u16)animationWord.bits;
    do {
        if (row->components[ordinal] != 0) {
            s32 actorSlot, rowField48, rowField4C;
            u16 modeHalf;
            u8 modeByte, actorRow;

            if (ordinal == 0) {
                slots[ordinal] = anchorSlot;
                actor = *(BriefingActor **)((u8 *)D_8022A974->actors + anchorOffset);
            } else {
                float x, z;

                slots[ordinal] = cloneSlot;
                actor = resource_alloc(sizeof(*actor));
                /* The retail write uses the scan result even when it reaches 28. */
                *(BriefingActor **)((u8 *)D_8022A974 + 0x18 +
                                    cloneSlot * (s32)sizeof(BriefingActor *)) = actor;
                func_00023780(actor, sizeof(*actor));
                *actor = *previous;
                x = actor->x - 8.0f;
                z = actor->z + 8.0f;
                actor->x = x;
                actor->z = z;
            }

            actorSlot = slots[ordinal];
            rowField48 = field48;
            rowField4C = field4C;
            actor->field145 = 1;
            actor->active = 1;
            modeHalf = (u16)mode;
            modeByte = (u8)mode;
            actorRow = (u8)rowIndex;
            actor->animation = animation;
            actor->state = animation;
            actor->rowIndex = actorRow;
            actor->sourceIndex = ordinal;
            actor->sequenceFlags = 0;
            actor->sequenceTimer = 0;
            actor->slot = actorSlot;
            actor->field0E8 = rowField48;
            actor->field0EC = rowField4C;
            actor->field13A = modeHalf;
            actor->field146 = modeByte;
            tag = (u16)func_00045934(row->field36, row->field38,
                                    row->field3A, row->field3C);
            func_00207658(&field48, &field4C, &mode, &mode, &tag, 1);
            func_8022E9E8(actor);
            previous = actor;
        }

        animation += 0x32;
        ordinal++;
    } while (ordinal < 3);
}
