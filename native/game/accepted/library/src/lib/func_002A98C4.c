typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

/* Local view of the pose fields queried through the scene's actor slots. */
typedef struct PoseActor {
    u8 pad_000[0xE8];
    s32 sourceArt;
    s32 ownerContext;
    u32 recordIndex;
    s32 countdown;
    u8 pad_0F8[0x3C];
    s16 pose;
    u8 pad_136[4];
    s16 field_13A;
    u8 pad_13C[0xA];
    u8 field_146;
} PoseActor;

typedef struct PoseScene {
    u8 pad_00[0x18];
    PoseActor *actors[28];
} PoseScene;

extern PoseScene *D_8022A974;
extern u32 func_00204F34(s32 sourceArt, s32 ownerContext,
                         s32 field146, s32 field13A, u32 pose, u32 index,
                         u8 *out0, u8 *out1, u8 *out2);

s32 func_002A98C4(s32 slot)
{
    PoseActor *actor;
    s32 token;

    actor = D_8022A974->actors[slot];
    /* The decoder returns a word. This query uses the low byte of each token. */
    token = (u8)func_00204F34(actor->sourceArt, actor->ownerContext,
                              actor->field_146, actor->field_13A,
                              actor->pose, actor->recordIndex, 0, 0, 0);
    if (token == 0) {
        return 0;
    }
    if (token == 1 &&
        (u8)func_00204F34(actor->sourceArt, actor->ownerContext,
                          actor->field_146, actor->field_13A,
                          actor->pose, actor->recordIndex + 1, 0, 0, 0) == 4 &&
        (u8)func_00204F34(actor->sourceArt, actor->ownerContext,
                          actor->field_146, actor->field_13A,
                          actor->pose, actor->recordIndex + 2, 0, 0, 0) == 0 &&
        actor->countdown < 2) {
        return 0;
    }
    return 1;
}
