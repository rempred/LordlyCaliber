typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

typedef struct {
    u8 pad_000[0xE4];
    s32 slot;
    s32 source;
    s32 pad_0EC;
    s32 sequence;
    s32 sequence_timer;
    s32 sequence_flags;
    u8 pad_0FC[0x20];
    f32 x;
    f32 y;
    f32 z;
    u8 pad_128[0x0C];
    s16 state;
    u8 pad_136[2];
    s16 animation;
    u8 pad_13A[3];
    s8 active;
    u8 pad_13E;
    u8 facing;
    s8 next_facing;
    u8 pad_141[5];
    u8 variant;
} Func002A08C0Actor;

typedef struct {
    u8 pad_00[0x18];
    Func002A08C0Actor *actors[1];
} Func002A08C0Scene;

extern Func002A08C0Scene * volatile D_8022A974;

s16 func_80231BBC(s32 source, s32 animation, s32 facing, s32 arg3);
void func_8009C970(Func002A08C0Actor *actor, s32 arg1, s32 arg2);
void func_8022E9E8(Func002A08C0Actor *actor);

s32 func_002A08C0(s32 slot, s32 source, s32 animation, s32 facing,
                  f32 x, f32 y, f32 z, s32 variant)
{
    Func002A08C0Actor *actor;
    register s32 actor_source asm("$18") = source;
    register s32 actor_animation asm("$19") = animation;
    register s32 actor_facing asm("$17") = facing;
    register s32 none = -1;
    f32 actor_y;
    f32 actor_x;
    f32 actor_z;

    actor_y = y;
    actor_x = x;
    actor_z = z;
    if (slot != none) {
        actor = D_8022A974->actors[slot];
        if (actor_source == none) {
            actor_source = actor->source;
        }
        if (actor_animation == none) {
            actor_animation = actor->animation;
        }
        if (actor_facing == none) {
            actor_facing = actor->facing;
        }
        if (variant == none) {
            variant = actor->variant;
        }

        if (func_80231BBC(actor_source, actor_animation, actor_facing, 0) != none) {
            actor->source = actor_source;
            actor->slot = slot;
            actor->animation = actor_animation;
            actor->state = func_80231BBC(actor->source, (s16)actor_animation,
                                         actor_facing, 0);
            actor->facing = actor_facing;
            actor->next_facing = actor_facing;
            actor->sequence = none;
            actor->sequence_timer = 0;
            actor->sequence_flags = 0;
            actor->variant = variant;
            actor->active = 0;

            if (((actor_x != -1.0f) || (actor_y != -1.0f)) ||
                (actor_y != -1.0f)) {
                actor->x = actor_x;
                actor->y = actor_y;
                actor->z = actor_z;
            }

            func_8009C970(actor, 0xFF, 0x10);
            func_8022E9E8(actor);
        }
    }
    return slot;
}
