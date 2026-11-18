typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

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

extern u8 D_801976E8;
extern u8 D_801971F2[][25];
extern BodyScene *D_8022A974;
extern BodyResident *D_801CE8BC;
extern s32 func_0020BFF8(BodyUnit *unit);

void func_002AD434(s32 destination, s32 memberPosition)
{
    u8 roster;
    u8 i;
    s32 destinationSlot;
    s32 rosterOffset;
    u8 *members;
    s32 wantedMember;
    BodyUnit *unit;
    u8 j;
    BodyScene *scene;
    BodyActor *moving;
    BodyActor *displaced;

    /* KMC schedules this byte load into the two-word entry preamble. */
    roster = D_801976E8;
    i = 0;
    destinationSlot = (u8)destination;
    rosterOffset = roster * 25;
    /* Address arithmetic preserves KMC's separate table-base addition. */
    members = (u8 *)(rosterOffset + (u32)D_801971F2);
    wantedMember = members[memberPosition];

    for (; i < 20; i++) {
        unit = &D_801CE8BC->units[i];
        if (func_0020BFF8(unit) == 0) {
            continue;
        }
        if (unit->memberIndex != wantedMember) {
            continue;
        }
        if (unit->sourceArt == 0) {
            continue;
        }

        scene = D_8022A974;
        for (j = 0; j < 0x1C; j++) {
            moving = scene->actors[j];
            if (moving == 0) {
                continue;
            }
            if (moving->unitIndex != i) {
                continue;
            }
            displaced = scene->actors[destinationSlot];
            scene->actors[j] = displaced;
            scene->actors[destinationSlot] = moving;
            moving->slot = destinationSlot;
            if (displaced != 0) {
                displaced->slot = j;
            }
            return;
        }
        return;
    }
}
