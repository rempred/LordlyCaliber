typedef struct Func001F0E64Record {
    int field_00;
    unsigned short field_04;
    short field_06;
    unsigned short field_08, field_0A;
    unsigned short field_0C, field_0E, field_10;
    unsigned char field_12[16];
    unsigned char field_22[16];
    unsigned char field_32[20];
    unsigned char field_46, field_47, field_48;
} Func001F0E64Record;

typedef struct TerminalPayload {
    unsigned char bytes[8];
} TerminalPayload;

typedef struct TerminalDispatchChild {
    void *word00;
    int scaledX, scaledY, scaledZ;
    unsigned char gap10[0xC];
    short currentX, currentY, currentZ;
    unsigned char gap22[6];
    short startX, startY, startZ;
    unsigned char gap2E[6];
    short value34;
    TerminalPayload payload;
    unsigned char gap3E[2];
    void *word40;
    Func001F0E64Record record;
    unsigned char gap90[4];
    int phase;
} TerminalDispatchChild;

/* The accepted compiler is big endian: low is the halfword at word+2. */
typedef union TerminalWordHalves {
    int word;
    struct { short high, low; } halves;
} TerminalWordHalves;

typedef struct TerminalDispatchContext {
    TerminalDispatchChild *first[3];
    TerminalDispatchChild *second[3];
    TerminalDispatchChild *extra18;
    TerminalDispatchChild *extra1C;
    unsigned char gap20[0x28];
    int sourceArt, ownerContext;
    unsigned char gap50[8];
    int grid58;
    TerminalWordHalves x, y, z;
    unsigned char gap68[0xC];
    int state;
    unsigned char gap78[0x2D];
    unsigned char activeMask;
    unsigned char byteA6;
} TerminalDispatchContext;

typedef struct TerminalUpdateContext TerminalUpdateContext;
typedef struct TerminalCopyState TerminalCopyState;
extern int D_801CE8C4;
extern unsigned int D_801CE8C8;
/* Only the signed byte reference is established; capacity is unknown. */
extern unsigned char D_801E5E0F[];
extern void func_001F7148(void);
void func_001F0E64(Func001F0E64Record *, unsigned int);
float func_001FF7EC(short, short);
int func_0020BFE4(void);
int func_0020C014(void *);
int func_0020C32C(void *);
void *func_0020C478(unsigned int);
void func_0020C6C4(void *, int *, int *);
void func_0020F3D4(void);
void func_0021E99C(TerminalCopyState *, TerminalCopyState *);
void func_0021EBBC(void);
int func_002213DC(TerminalUpdateContext *);

/* Keep both actual wobble bodies, including their per-child side queries. */
#define WOBBLE_CHILDREN() \
    for (i = 0; i < 3u; i++) { \
        if (((context->activeMask >> i) & 1) != 0) { \
            if (context->first[i] != 0) { \
                first = context->first[i]; \
                phase = first->phase; \
                if (phase < 3) { \
                    x = first->currentX; \
                    first->currentX = func_0020C014(context) ? x + 2 : x - 2; \
                } else if (phase < 6) { \
                    x = first->currentX; \
                    first->currentX = func_0020C014(context) ? x - 2 : x + 2; \
                } \
                first->phase = (unsigned int)first->phase + 1u; \
            } \
        } \
    }

/* Propagation precedes the clamp and must reload the stored halfword. */
#define PROPAGATE_VALUE(owner, child) \
    if ((owner)->extra18 != 0) (owner)->extra18->value34 = (child)->value34; \
    if ((owner)->extra1C != 0) (owner)->extra1C->value34 = (child)->value34

/* The final test of extra1C really stores through extra18. */
#define CLAMP_EXTRA_VALUE(owner, value) \
    if ((owner)->extra18 != 0) (owner)->extra18->value34 = (value); \
    if ((owner)->extra1C != 0) (owner)->extra18->value34 = (value)

void func_002215D0(void)
{
    int output0, output1;
    int otherOutput0, otherOutput1;
    int index;
    unsigned int i;
    TerminalDispatchContext *context;

    if (D_801CE8C4 != 0) {
        func_0021EBBC();
        D_801CE8C8++;
    }
    for (index = 0; index < 20; index++) {
        context = func_0020C478(index);
        if (context == 0) continue;
        switch (context->state) {
        case 1:
            break;
        case 6: {
            TerminalDispatchChild *first;
            for (i = 0; i < 3u; i++) {
                first = context->first[i];
                if (first != 0 && (short)first->record.field_04 < 5)
                    context->state = 1;
            }
            break;
        }
        case 2:
        case 8:
        case 33:
            if (func_002213DC((TerminalUpdateContext *)context) != 0)
                context->state = 1;
            break;
        case 10: {
            TerminalDispatchChild *first; int phase, flag;
            flag = 0;
            for (i = 0; i < 3u; i++) {
                first = context->first[i];
                if (first != 0) {
                    phase = first->phase;
                    first->phase = (unsigned int)phase - 1u;
                    if (phase == 0) flag = 1;
                }
            }
            if (func_002213DC((TerminalUpdateContext *)context) != 0 || flag != 0) {
                for (i = 0; i < 3u; i++) {
                    if (((context->activeMask >> i) & 1) != 0 && context->first[i] != 0) {
                        func_001F0E64(&context->first[i]->record, 12);
                        func_0021E99C((TerminalCopyState *)context->first[i],
                                     (TerminalCopyState *)context->second[i]);
                    }
                }
                context->state = 1;
            }
            break;
        }
        case 12: {
            TerminalDispatchChild *first; int phase, x;
            if (func_0020C32C(context) != 0) {
                WOBBLE_CHILDREN();
            }
            break;
        }
        case 50: {
            int flag; unsigned int j;
            flag = 0;
            if (func_0020C014(context) != 0) {
                for (j = 0; j < 3u; j++) {
                    TerminalDispatchChild *first;
                    first = context->first[j];
                    if (first != 0) {
                        first->currentX = (unsigned short)first->currentX + 8;
                        if (first->currentX - first->startX >= 38) {
                            first->currentX = first->startX + 38;
                            flag = 1;
                        }
                        first->scaledX = first->currentX * 30;
                        context->second[j]->currentX = first->currentX;
                    }
                }
                if (flag != 0) {
                    context->x.word = (unsigned int)context->x.word + 38u;
                    context->grid58 = (unsigned int)context->grid58 + 1u;
                    context->y.word = (int)func_001FF7EC(context->x.halves.low, context->z.halves.low);
                    for (j = 0; j < 3u; j++) {
                        TerminalDispatchChild *first, *second, *resetFirst, *resetSecond; int x, value;
                        first = context->first[j];
                        if (first != 0) {
                            second = context->second[j];
                            x = (unsigned short)first->currentX;
                            second->startX = x;
                            first->startX = x;
                            resetSecond = context->second[j];
                            value = (unsigned short)context->y.halves.low;
                            resetFirst = context->first[j];
                            resetSecond->currentY = value;
                            resetFirst->currentY = value;
                            resetSecond->startY = value;
                            resetFirst->startY = value;
                        }
                    }
                }
            } else {
                for (j = 0; j < 3u; j++) {
                    TerminalDispatchChild *first;
                    first = context->first[j];
                    if (first != 0) {
                        first->currentX = (unsigned short)first->currentX - 8;
                        if (first->startX - first->currentX >= 38) {
                            first->currentX = first->startX - 38;
                            flag = 1;
                        }
                        first->scaledX = first->currentX * 30;
                        context->second[j]->currentX = first->currentX;
                    }
                }
                if (flag != 0) {
                    context->x.word = (unsigned int)context->x.word - 38u;
                    context->grid58 = (unsigned int)context->grid58 - 1u;
                    context->y.word = (int)func_001FF7EC(context->x.halves.low, context->z.halves.low);
                    for (j = 0; j < 3u; j++) {
                        TerminalDispatchChild *first, *second, *resetFirst, *resetSecond; int x, value;
                        first = context->first[j];
                        if (first != 0) {
                            second = context->second[j];
                            x = (unsigned short)first->currentX;
                            second->startX = x;
                            first->startX = x;
                            resetSecond = context->second[j];
                            value = (unsigned short)context->y.halves.low;
                            resetFirst = context->first[j];
                            resetSecond->currentY = value;
                            resetFirst->currentY = value;
                            resetSecond->startY = value;
                            resetFirst->startY = value;
                        }
                    }
                }
            }
            if (flag != 0) context->state = 1;
            break;
        }
        case 45: {
            TerminalDispatchChild *first; int phase, x;
            WOBBLE_CHILDREN();
            break;
        }
        case 19: {
            TerminalDispatchChild *first, *second; int phase, value;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        second = context->second[i];
                        phase = first->phase;
                        if ((first != 0) & (second != 0)) {
                            if (first->value34 != 0) {
                                first->value34 = (unsigned int)first->value34 - (unsigned int)phase;
                                second->value34 = first->value34 / 4;
                                if (first->value34 < 0) {
                                    second->value34 = 0;
                                    first->value34 = 0;
                                }
                            }
                        }
                    }
                }
            }
            break;
        }
        case 35: {
            TerminalDispatchChild *first; int phase;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        func_0020C6C4(&first->record, &output0, &output1);
                        if ((short)first->record.field_0A == output1) {
                            phase = (unsigned int)first->phase + 12u;
                            if (phase < 256) {
                                first->phase = phase;
                                first->record.field_08 = 3;
                            } else {
                                first->phase = 0;
                                /* (a | phase) & a equals a. With pinned KMC this direct
                                 * form keeps the shared address invariant and its allocation
                                 * priority; the arithmetic disappears before emission. A named
                                 * address temporary instead moves the loads into each case. */
                                first->word00 = (void *)(
                                    ((unsigned int)func_001F7148 | (unsigned int)phase)
                                    & (unsigned int)func_001F7148);
                                context->state = 1;
                                first->record.field_08 = 0;
                            }
                        }
                    }
                }
            }
            break;
        }
        case 36: {
            TerminalDispatchChild *first; Func001F0E64Record *record;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        record = &first->record;
                        func_0020C6C4(record, &otherOutput0, &otherOutput1);
                        if ((int)((unsigned int)first->phase + 12u) < 256) {
                            if (func_0020BFE4() != 0)
                                first->phase = (unsigned int)first->phase + 12u;
                            else
                                first->phase = (unsigned int)first->phase + 64u;
                            record->field_08 = 3;
                        } else {
                            first->phase = 0;
                            first->word00 = (void *)func_001F7148;
                            context->state = 1;
                            first->record.field_08 = 0;
                        }
                    }
                }
            }
            break;
        }
        case 46: {
            TerminalDispatchChild *first; int phase;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        first->currentY = (unsigned short)first->currentY + (signed char)D_801E5E0F[first->phase];
                        phase = (unsigned int)context->first[i]->phase - 1u;
                        context->first[i]->phase = phase;
                        if (phase == 0) context->state = 1;
                    }
                }
            }
            break;
        }
        case 47: {
            TerminalDispatchChild *first; int phase;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        first->currentY = (unsigned short)first->currentY - (signed char)D_801E5E0F[first->phase];
                        phase = (unsigned int)context->first[i]->phase - 1u;
                        context->first[i]->phase = phase;
                        if (phase == 0) {
                            context->state = 1;
                            func_001F0E64(&context->first[i]->record, 0);
                        }
                    }
                }
            }
            break;
        }
        case 57: {
            TerminalDispatchContext *target; TerminalDispatchChild *first, *second; int value;
            target = func_0020C478(context->byteA6);
            {
                TerminalDispatchChild *payloadSource = context->first[0];
                if (payloadSource->payload.bytes[1] != 0)
                    target->first[0]->payload = payloadSource->payload;
            }
            first = context->first[0];
            second = context->second[0];
            if (first->value34 != 0) {
                first->value34 = (unsigned int)first->value34 - (unsigned int)first->phase;
                second->value34 = first->value34 / 4;
                PROPAGATE_VALUE(context, first);
                if (first->value34 < 0) {
                    second->value34 = 0;
                    first->value34 = 0;
                    CLAMP_EXTRA_VALUE(context, 0);
                }
            }
            first = target->first[0];
            second = target->second[0];
            if (first->value34 != 255) {
                first->value34 = (unsigned int)first->value34 + (unsigned int)first->phase;
                second->value34 = first->value34 / 4;
                PROPAGATE_VALUE(target, first);
                if (first->value34 >= 256) {
                    first->value34 = 255;
                    second->value34 = 63;
                    CLAMP_EXTRA_VALUE(target, 255);
                }
            }
            break;
        }
        case 59: {
            TerminalDispatchChild *first, *second; int value;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        second = context->second[i];
                        if (first->value34 != 255) {
                            first->value34 = (unsigned int)first->value34 + (unsigned int)first->phase;
                            second->value34 = first->value34 / 4;
                            PROPAGATE_VALUE(context, first);
                            if (first->value34 >= 256) {
                                first->value34 = 255;
                                second->value34 = 63;
                                CLAMP_EXTRA_VALUE(context, 255);
                            }
                        }
                    }
                }
            }
            context->byteA6++;
            if (context->byteA6 == 35) {
                for (i = 0; i < 3u; i++) {
                    if (((context->activeMask >> i) & 1) != 0 && context->first[i] != 0)
                        func_001F0E64(&context->first[i]->record, 40);
                }
                context->state = 1;
            }
            break;
        }
        case 60: {
            TerminalDispatchChild *first, *second; int value;
            for (i = 0; i < 3u; i++) {
                if (((context->activeMask >> i) & 1) != 0) {
                    first = context->first[i];
                    if (first != 0) {
                        second = context->second[i];
                        if (first->value34 != 0) {
                            first->value34 = (unsigned int)first->value34 - (unsigned int)first->phase;
                            second->value34 = first->value34 / 4;
                            PROPAGATE_VALUE(context, first);
                            if (first->value34 < 0) {
                                second->value34 = 0;
                                first->value34 = 0;
                                CLAMP_EXTRA_VALUE(context, 0);
                            }
                        }
                    }
                }
            }
            func_002213DC((TerminalUpdateContext *)context);
            break;
        }
        }
    }
    func_0020F3D4();
}
