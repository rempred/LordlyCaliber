typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;

typedef struct BodyUnit {
    u8 pad00[0x48];
    s32 sourceArt;
    u8 pad4C[0xAA];
    u8 memberIndex;
    u8 padF7;
} BodyUnit;

typedef struct BodyActor {
    u8 pad000[0xE4];
    s32 slot;
    u8 pad0E8[0x50];
    s16 animation;
    u8 pad13A[0xD];
    u8 unitIndex;
} BodyActor;

typedef struct BodyScene {
    u8 pad00[0x18];
    BodyActor *actors[28];
} BodyScene;

typedef struct BodyResident {
    u8 pad000[0x1C4];
    BodyUnit units[20];
} BodyResident;

extern BodyScene *D_8022A974;
extern BodyResident *D_801CE8BC;
extern s32 func_0020C120(BodyUnit *unit);
extern s32 func_0020C0E8(BodyUnit *unit);
extern s32 func_0020BFF8(BodyUnit *unit);

void func_002AB574(void)
{
    s32 i;
    BodyActor *actor;
    BodyUnit *unit;

    for (i = 0; i < 0x1C; i++) {
        actor = D_8022A974->actors[i];
        if (actor == 0) {
            continue;
        }
        unit = &D_801CE8BC->units[actor->unitIndex];
        if (func_0020C120(unit) == 1 && actor->animation >= 0x32) {
            continue;
        }
        if (func_0020C0E8(unit) == 0) {
            continue;
        }

        /* Keep temporaries local to each swap for KMC register allocation. */
        if (func_0020BFF8(unit) != 0) {
            BodyScene *scene;
            s32 slot;
            BodyActor *moving;
            BodyActor *displaced;

            if (i == 1) {
                continue;
            }
            scene = D_8022A974;
            slot = (u8)i;
            moving = scene->actors[slot];
            displaced = scene->actors[1];
            scene->actors[1] = moving;
            scene->actors[slot] = displaced;
            if (displaced != 0) {
                displaced->slot = slot;
            }
            if (moving != 0) {
                moving->slot = 1;
            }
        } else {
            BodyScene *scene;
            s32 slot;
            BodyActor *moving;
            BodyActor *displaced;

            if (i == 0) {
                continue;
            }
            scene = D_8022A974;
            slot = (u8)i;
            moving = scene->actors[slot];
            displaced = scene->actors[0];
            scene->actors[0] = moving;
            scene->actors[slot] = displaced;
            if (displaced != 0) {
                displaced->slot = slot;
            }
            if (moving != 0) {
                moving->slot = 0;
            }
        }
    }
}
