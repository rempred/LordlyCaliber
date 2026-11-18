typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
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
extern void *resource_alloc(u32 bytes);
extern s32 func_0020BFF8(BodyUnit *unit);
extern s32 func_0020C0E8(BodyUnit *unit);
extern s32 func_0020C120(BodyUnit *unit);
extern s16 func_002A0A68(BodyActor *actor, f32 x, f32 z, f32 targetX, f32 targetZ);
extern s32 func_002A9364(s32 slot, s32 sourceArt, s32 ownerContext, s32 animation,
                        s32 arg5, s32 arg6, u8 keep145, u8 keepUnit);

void func_002ACFD8(s32 mode)
{
    u8 *states;
    s32 i;
    s32 filter;
    BodyActor *actor;
    BodyUnit *unit;
    BodySource *source;
    s32 isLeader;
    s32 animation;

    /* KMC schedules this global load into the two-word entry preamble. */
    states = D_8022A974->states;
    if (states == 0) {
        u8 n;

        states = resource_alloc(0x1D);
        D_8022A974->states = states;
        for (n = 0; n < 0x1C; n++) {
            states[n] = 4;
        }
    }
    states[0x1C] = 0;
    filter = mode % 3;

    for (i = 0; i < 0x1C; i++) {
        actor = D_8022A974->actors[i];
        if (actor == 0) {
            continue;
        }
        if (actor->unitIndex == 0xFF) {
            continue;
        }
        unit = &D_801CE8BC->units[actor->unitIndex];
        isLeader = func_0020BFF8(unit) != 0;
        if (mode < 3) {
            if (isLeader == 0) {
                continue;
            }
        } else if (isLeader == 1) {
            continue;
        }
        if (filter == 1) {
            if (func_0020C0E8(unit) == 0) {
                continue;
            }
        } else if (filter == 2 && func_0020C0E8(unit) != 0) {
            continue;
        }
        source = unit->sources[actor->sourceIndex];
        if ((s16)func_002A0A68(actor, actor->x, actor->z, source->x, source->z) == 3) {
            continue;
        }

        if (func_0020C120(unit) == 1) {
            animation = (actor->sourceIndex == 1 ? 0x3B : 0) | 9;
        } else {
            animation = 9;
        }
        func_002A9364(i, unit->sourceArt, unit->ownerContext, animation, isLeader, isLeader, 0, 1);
        states[i] = 1;
    }
}
