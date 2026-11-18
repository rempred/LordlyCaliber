typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

enum {
    SCENE_SLOT_COUNT = 28,
    UNASSIGNED_ROW = 0xFF,
    SECONDARY_ANIMATION_BASE = 50
};

/* Partial views of the scene slots and deployed rows used by 2AAC60. */
typedef struct Actor {
    u8 pad_000[0xE0];
    void *descriptor;
    s32 slot;
    s32 classId;
    s32 variant;
    s32 sequence;
    s32 sequenceTimer;
    s32 sequenceFlags;
    u8 pad_0FC[0x2C];
    s32 field_128;
    s32 field_12C;
    f32 field_130;
    s16 state;
    u8 pad_136[2];
    s16 animation;
    s16 field_13A;
    u8 field_13C;
    u8 active;
    u8 pad_13E[7];
    u8 field_145;
    u8 field_146;
    u8 unitIndex;
    u8 field_148;
    u8 sourceIndex;
} Actor;

typedef struct Scene {
    u8 pad_00[0x18];
    Actor *actors[SCENE_SLOT_COUNT];
} Scene;

typedef struct BodyUnit {
    u8 pad_00[0x36];
    u16 equipment[4];
    u8 pad_3E[0xA];
    s32 classId;
    s32 variant;
    u8 pad_50[0xA8];
} BodyUnit;

typedef struct Resident {
    u8 pad_000[0x1C4];
    /* This view supplies the row stride. The observed index is a full byte. */
    BodyUnit units[0];
} Resident;

extern Scene *D_8022A974;
extern Resident *D_801CE8BC;
extern s32 D_801CE8FC;
extern u8 D_8022F2CC;
extern s32 func_0020C120(BodyUnit *unit);
extern void *func_0002CD70(void *destination, s32 value, u32 count);
extern void func_00023780(void *destination, u32 count);
extern u16 func_00043e88(s32 classId, s32 variant);
extern u16 func_00043edc(s32 classId, s32 variant);
extern u16 func_00043f30(s32 classId, s32 variant);
extern u16 func_00043f84(s32 classId, s32 variant);
extern s32 func_00045934(s32 item0, s32 item1, s32 item2, s32 item3);
extern void func_00207658(s32 *, s32 *, s32 *, s32 *, s32 *, u32 count);
extern void func_8022E9E8(Actor *actor);

static __inline__ u8 classify_body(Actor *actor, Actor *parent, s32 checkParent)
{
    BodyUnit *unit;
    s32 kind;
    u8 unitIndex;

    if (actor != 0 && (unitIndex = actor->unitIndex) != UNASSIGNED_ROW) {
        /* Keep this guard separate to preserve KMC's sentinel register allocation. */
        if (checkParent && unitIndex != parent->unitIndex) {
            return 0;
        }
        unit = &D_801CE8BC->units[unitIndex];
        if (unit->classId != 0) {
            kind = func_0020C120(unit);
            if (kind == 1) {
                if (actor->sourceIndex == kind) {
                    return 2;
                }
                return 1;
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

static __inline__ u8 find_unused_index(void)
{
    u8 row, scan;
    Actor *other;

    for (row = 0; row < SCENE_SLOT_COUNT; row++) {
        for (scan = 0; scan < SCENE_SLOT_COUNT; scan++) {
            other = D_8022A974->actors[scan];
            if (other != 0 && other->unitIndex == row) {
                break;
            }
        }
        if (scan >= SCENE_SLOT_COUNT) {
            return row;
        }
    }
    return UNASSIGNED_ROW;
}

s32 func_002A9364(s32 slot, s32 requestedClass, s32 requestedVariant,
                  s32 requestedAnimation, s32 requested13A, s32 requested146,
                  u8 keep145, u8 keepUnit)
{
    s32 classId;
    s32 variant;
    s32 field13A;
    s32 field146;
    s32 equipment;
    Actor *actor;
    BodyUnit *unit;
    s32 animation;
    u8 kind;
    s16 i;
    Actor *other;

    field13A = requested13A;
    animation = requestedAnimation;
    classId = requestedClass;
    equipment = 0;
    variant = requestedVariant;
    field146 = requested146;

    if (slot == -1) {
        goto done;
    }
    actor = D_8022A974->actors[slot];
    if (actor == 0) {
        return slot;
    }
    if (requestedClass == -1) {
        classId = actor->classId;
    }
    if (requestedAnimation == -1) {
        animation = actor->animation;
    }
    if (requested13A == -1) {
        field13A = actor->field_13A;
    }
    if (requestedVariant == -1) {
        variant = actor->variant;
    }

    kind = classify_body(actor, 0, 0);
    if (kind == 2) {
        if (animation >= SECONDARY_ANIMATION_BASE) {
            return slot;
        }
        animation += SECONDARY_ANIMATION_BASE;
    }

    actor->slot = (u8)slot;
    if (classId != -1) {
        actor->classId = classId;
    }
    if (animation != -1) {
        actor->animation = animation;
    }
    if (variant != -1) {
        actor->variant = variant;
    }
    actor->active = 1;
    actor->state = animation;
    actor->sequence = -1;
    actor->sequenceTimer = 0;
    actor->sequenceFlags = 0;
    actor->field_12C = 0;
    actor->descriptor = &D_8022F2CC;
    /* Retail repeats the field reset around descriptor initialization. */
    actor->field_12C = 0;
    actor->field_128 = 0;
    actor->field_13A = field13A;
    actor->field_130 = 1.0f;
    actor->field_13C = D_801CE8FC;
    if (keep145 == 0) {
        actor->field_145 = 1;
    }
    func_0002CD70(actor, 0xFF, 0x10);
    func_00023780((u8 *)actor + 0x10, 0x10);
    if (field146 != -1) {
        actor->field_146 = field146;
    }
    field146 = actor->field_146;

    if (keepUnit == 0 && actor->unitIndex == UNASSIGNED_ROW) {
        actor->unitIndex = find_unused_index();
        actor->field_148 = 1;
        unit = &D_801CE8BC->units[actor->unitIndex];
        unit->equipment[0] = func_00043e88((u8)classId, (u8)variant);
        unit->equipment[1] = func_00043edc((u8)classId, (u8)variant);
        unit->equipment[2] = func_00043f30((u8)classId, (u8)variant);
        unit->equipment[3] = func_00043f84((u8)classId, (u8)variant);
    } else {
        unit = &D_801CE8BC->units[actor->unitIndex];
    }
    equipment = (u16)func_00045934(unit->equipment[0], unit->equipment[1],
                                   unit->equipment[2], unit->equipment[3]);
    func_00207658(&classId, &variant, &field13A, &field146, &equipment, 1);
    func_8022E9E8(actor);

    kind = classify_body(actor, 0, 0);
    if (kind == 1) {
        i = 0;
        do {
            other = D_8022A974->actors[i];
            kind = classify_body(other, actor, 1);
            if (kind == 2) {
                /* Each linked slot resolves its own inherited inputs. */
                func_002A9364(i, requestedClass, requestedVariant, requestedAnimation,
                              requested13A, requested146, keep145, keepUnit);
            }
            i++;
        } while (i < SCENE_SLOT_COUNT);
    }
done:
    return slot;
}

