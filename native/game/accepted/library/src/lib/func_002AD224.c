typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

/* Partial views of the deployed row, its coordinate sources and scene actor. */
typedef struct BodySource {
    u8 pad00[0x28];
    s16 x, y, z;
} BodySource;

typedef struct BodyUnit {
    BodySource *sources[3];
    u8 pad0C[0x3C];
    s32 sourceArt;
    s32 ownerContext;
    u8 pad50[0xA8];
} BodyUnit;

typedef struct BodyActor {
    u8 pad000[0xE4];
    s32 slot;
    u8 pad0E8[0x34];
    f32 x, y, z;
    u8 pad128[0x10];
    s16 animation;
    u8 pad13A[0xD];
    u8 unitIndex;
    u8 pad148;
    u8 sourceIndex;
} BodyActor;

typedef struct BodyScene {
    u8 pad0000[0x18];
    BodyActor *actors[28];
    u8 pad0088[0x1C20];
    u8 *states;
} BodyScene;

typedef struct BodyResident {
    u8 pad000[0x1C4];
    /* The actor supplies the row index. This trailing view supplies its stride. */
    BodyUnit units[0];
} BodyResident;

extern BodyScene *D_8022A974;
extern BodyResident *D_801CE8BC;
extern s32 resource_free(void *pointer);
extern s32 func_0020C120(BodyUnit *unit);
extern s32 func_002A98C4(s32 slot);
extern s32 func_002A13C4(s32 slot);
extern s32 func_002AB0F8(BodyActor *actor, BodyUnit *unit);
extern s32 func_002A9364(s32 slot, s32 sourceArt, s32 ownerContext, s32 animation,
                        s32 arg5, s32 arg6, u8 keep145, u8 keepUnit);

void func_002AD224(void)
{
    u8 *initialStates;
    u8 *states;
    u8 anyActive;
    s16 i;
    s32 slot;
    u8 *state;
    BodyActor *actor;
    BodyUnit *unit;
    s16 animation;

    /* Keep the initial null check separate from the loop's state pointer. */
    initialStates = D_8022A974->states;
    anyActive = 0;
    if (initialStates == 0) {
        return;
    }

    states = initialStates;
    for (i = 0; i < 0x1C; i++) {
        slot = i;
        state = &states[slot];
        if (*state == 4) {
            continue;
        }
        actor = D_8022A974->actors[slot];
        anyActive = 1;
        animation = 0;
        unit = &D_801CE8BC->units[actor->unitIndex];
        if (func_0020C120(unit) == 1) {
            animation = actor->sourceIndex == 1 ? 0x32 : 0;
        }
        /* Keep both refresh paths explicit for KMC's common-tail merge.
         * The extra animation offset belongs only to state 2. */
        switch (*state) {
            case 5:
                if (func_002A98C4(slot) != 0) {
                    continue;
                }
                func_002A9364(slot, -1, -1, animation, -1, -1, 0, 1);
                *state = 4;
                break;
            case 1:
                if (func_002A98C4(slot) == 0) {
                    *state = 2;
                    func_002AB0F8(actor, unit);
                }
                break;
            case 2:
                if (func_002A13C4(slot) != 0) {
                    continue;
                }
                if (states[0x1C] == 0) {
                    animation += 8;
                }
                func_002A9364(slot, -1, -1, animation, -1, -1, 0, 1);
                *state = 4;
                break;
        }
    }
    if (!anyActive) {
        resource_free(states);
        D_8022A974->states = 0;
    }
}
