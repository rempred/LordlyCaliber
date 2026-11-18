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

extern s32 func_0020BFF8(BodyUnit *unit);
extern s32 func_0020C120(BodyUnit *unit);
extern f32 func_00201670(s32 sourceArt, s32 ownerContext);
extern f32 func_002016F4(s32 sourceArt, s32 ownerContext);
extern void func_002A0B14(s32 slot, f32 startX, f32 startZ, f32 targetX,
                         f32 targetZ, s32 duration, s32 scaledDuration);
extern s32 func_002A9364(s32 slot, s32 sourceArt, s32 ownerContext, s32 animation,
                        s32 arg5, s32 arg6, u8 keep145, u8 keepUnit);

s32 func_002AB0F8(BodyActor *actor, BodyUnit *unit)
{
    BodySource *source;
    s32 x, z;
    s16 animation;
    u8 isLeader;
    f32 scale, work;
    s32 scaledDuration;
    s32 paired;

    isLeader = func_0020BFF8(unit) != 0;
    source = unit->sources[actor->sourceIndex];
    x = source->x;
    z = source->z;
    if (isLeader == 1) {
        animation = actor->x < (f32)x ? 5 : 11;
    } else {
        animation = (f32)x < actor->x ? 5 : 11;
    }

    if (animation == 5) {
        scale = func_00201670(unit->sourceArt, unit->ownerContext);
    } else {
        scale = func_002016F4(unit->sourceArt, unit->ownerContext);
    }
    /* Reuse the floating temporary for the multiplier and the Z argument.
     * KMC then keeps the product in the floating return register. */
    work = 555.0f;
    scaledDuration = (s32)(scale * work);
    work = z;
    func_002A0B14(actor->slot, -1.0f, -1.0f, x, work, 0, scaledDuration);
    paired = func_0020C120(unit);
    if (paired == 1 && actor->sourceIndex == paired) {
        animation += 0x32;
    }
    func_002A9364(actor->slot, unit->sourceArt, unit->ownerContext, animation,
                  isLeader, isLeader, 0, 1);
    return 1;
}
