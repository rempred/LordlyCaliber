typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef float f32;

typedef struct PoseSource {
    u8 pad00[0x44];
    u8 state;
} PoseSource;

typedef struct PoseRow {
    PoseSource *sources[3];
    u8 pad0C[0x3C];
    s32 sourceArt;
    s32 ownerContext;
    u8 pad50[0xA8];
} PoseRow;

typedef struct PoseResident {
    u8 pad000[0x1C4];
    /* Partial view of the row array indexed by the actor's unit byte. */
    PoseRow rows[0];
} PoseResident;

typedef struct Vec3f {
    f32 x, y, z;
} Vec3f;

typedef struct PoseActor {
    u8 channels[16];
    s8 deltas[16];
    u8 pad020[0x80];
    u8 transform[0x48];
    s32 sourceArt;
    s32 ownerContext;
    s32 record;
    s32 delay;
    s32 field0F8;
    u8 pad0FC[0x20];
    f32 x, y, z;
    u8 pad128[0xC];
    s16 sequence;
    s16 previousSequence;
    u8 pad138[2];
    s16 field13A;
    u8 pad13C;
    u8 alternate;
    u8 pad13E[8];
    u8 selector;
    u8 unitIndex;
    u8 pad148;
    u8 sourceIndex;
} PoseActor;

extern PoseResident *D_801CE8BC;
extern u16 D_801CF3F0[], D_801CF3F2[], D_801CF3F4[];
extern u16 D_801CEF10[], D_801CEF12[];
extern const u8 g_func_0015DF10_match[], g_func_0015DF10_default[];
extern const struct {
    u8 prefix[0xC0];
    Vec3f position;
} float_002a89b0;
extern s32 func_002a3198(s32, s32, s32, s32, u8 *, u8 *, u8 *);
extern u32 func_00204F34(s32, s32, s32, s32, u32, u32, u8 *, u8 *, u8 *);
extern void func_00208DC8(const void *, s32);
extern void func_0029CD64(Vec3f *, f32 *, void *);
extern void func_0029DF5C(void *, s32, s32, s32, s32, s32);
extern void *func_0002CD70(void *, s32, u32);

u8 func_0029E218(PoseActor *actor)
{
    /* Keep the three decoder outputs contiguous ahead of the vectors on the KMC stack. */
    struct {
        u8 first, second, third;
    } operands;
    Vec3f position;
    f32 projected[2];
    s32 code;
    s32 positionMatchCommand;
    const u16 *eventTable;
    s16 testArt;
    u8 i;
    s16 sum;

    code = 1;
    if (actor->delay <= 0) {
        /* KMC loads the event table base before the shared command ID. */
        eventTable = D_801CF3F0;
        positionMatchCommand = 18;
        do {
            ++actor->record;
            if (actor->alternate == 0) {
                code = func_002a3198(actor->sourceArt, actor->sequence,
                                    actor->record, actor->selector,
                                    &operands.first, &operands.second, &operands.third);
            } else {
                /* The explicit shared arm preserves KMC's three comparison branches. */
                testArt = actor->sourceArt;
                if (testArt == 0x87) {
                    goto specialArt;
                }
                if (testArt == 0xA1) {
                    goto specialArt;
                }
                if (testArt == 0x88) {
specialArt:
                    testArt = -1;
                }
                if (testArt == -1) {
                    if (actor->sequence >= 0x32) {
                        actor->sequence = 0x32;
                    } else {
                        actor->sequence = 0;
                    }
                }
                code = func_00204F34(actor->sourceArt, actor->ownerContext,
                                    actor->selector, actor->field13A, actor->sequence,
                                    actor->record, &operands.first,
                                    &operands.second, &operands.third);
            }

            switch (code & 0xFF) {
            case 1: {
                s32 stepValue = operands.first;
                s32 delay = operands.second;
                actor->field0F8 = stepValue;
                actor->delay = delay;
                break;
            }
            case 21: {
                s32 stepValue = operands.first + (operands.second << 8);
                s32 delay = operands.third;
                actor->field0F8 = stepValue;
                actor->delay = delay;
                break;
            }
            case 4:
                actor->record = operands.first - 1;
                break;
            case 5: {
                s16 previous = actor->sequence;
                s16 sequence = operands.first;
                actor->record = -1;
                actor->previousSequence = previous;
                actor->sequence = sequence;
                break;
            }
            case 3:
                actor->delay = operands.first;
                break;
            case 2: {
                f32 x = actor->x - (s8)operands.first;
                f32 z = actor->z - (s8)operands.second;
                actor->x = x;
                actor->z = z;
                break;
            }
            case 12: {
                f32 x = actor->x + (s8)operands.first;
                f32 y = actor->y + (s8)operands.second;
                f32 z = actor->z + (s8)operands.third;
                actor->x = x;
                actor->y = y;
                actor->z = z;
                break;
            }
            case 17:
                func_00208DC8(g_func_0015DF10_match,
                              eventTable[(operands.first << 8) + operands.second]);
                break;
            case 19:
                func_00208DC8(g_func_0015DF10_default,
                              eventTable[(operands.first << 8) + operands.second]);
                break;
            case 18:
            case 20:
                if (actor->alternate == 0) {
                    const void *recipient;
                    s32 index;
                    s32 positionX;
                    s32 value;

                    position = float_002a89b0.position;
                    func_0029CD64(&position, projected, actor->transform);
                    positionX = (s32)position.x;
                    index = (operands.first << 8) + operands.second;
                    if ((u32)(index - 0x24E) < 0xAB) {
                        recipient = g_func_0015DF10_default;
                        if ((code & 0xFF) == positionMatchCommand) {
                            recipient = g_func_0015DF10_match;
                        }
                        if (positionX >= 213) {
                            value = eventTable[index];
                        } else if (positionX < 106) {
                            value = D_801CF3F4[index];
                        } else {
                            value = D_801CF3F2[index];
                        }
                    } else {
                        recipient = g_func_0015DF10_default;
                        if ((code & 0xFF) == positionMatchCommand) {
                            recipient = g_func_0015DF10_match;
                        }
                        if (positionX < 160) {
                            value = D_801CEF10[index * 2];
                        } else {
                            value = D_801CEF12[index * 2];
                        }
                    }
                    func_00208DC8(recipient, value);
                } else {
                    PoseRow *row = &D_801CE8BC->rows[actor->unitIndex];
                    PoseSource *source = row->sources[actor->sourceIndex];
                    if (source != 0) {
                        func_0029DF5C(&source->state, row->sourceArt, row->ownerContext,
                                      operands.first, operands.second, code & 0xFF);
                    }
                }
                break;
            case 13:
                if (operands.first == 0xFF) {
                    func_0002CD70(actor->channels, operands.second, 16);
                } else {
                    actor->channels[operands.first] = operands.second;
                }
                break;
            case 16:
                if (operands.first == 0xFF) {
                    func_0002CD70(actor->deltas, operands.second, 16);
                } else {
                    actor->deltas[operands.first] = operands.second;
                }
                break;
            case 0:
                actor->delay = 2;
                break;
            }
        } while (actor->delay <= 0);
    }

    for (i = 0; i < 16; i++) {
        sum = actor->channels[i] + actor->deltas[i];
        if (sum < 0) {
            actor->channels[i] = 0;
        } else if (sum >= 256) {
            actor->channels[i] = 255;
        } else {
            actor->channels[i] = sum;
        }
    }
    actor->delay -= 2;
    return code & 0xFF;
}
