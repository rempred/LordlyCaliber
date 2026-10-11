/* Private whole-owner reconstruction. A matching command stays in the loop;
 * mismatch counters form the separate final branch.
 * ROM 21EBBC..2213DC includes the 988-byte continuation.
 * Raw command and field names do not assert gameplay meaning. */
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;

typedef struct Func001F0E64Record {
    int field_00;
    u16 field_04;
    short field_06;
    u16 field_08, field_0A, field_0C, field_0E, field_10;
    u8 field_12[16], field_22[16], field_32[20];
    u8 field_46, field_47, field_48;
} Func001F0E64Record;

/* Identical to the accepted anonymous state type in func_001F197C.c. */
typedef struct {
    int field00;
    u16 field04;
    s16 field06, field08, field0A, field0C, field0E, field10;
    u8 field12[16];
    s8 field22[16];
    u8 field32[4], field36[4], field3A[4], field3E[4], field42[4];
    u8 field46, field47;
} Func001F197CState;

typedef union TerminalCommandPose {
    struct { Func001F0E64Record record; u8 tail4C[4]; } view;
    struct { Func001F197CState state; u8 tail48[8]; } calculation;
    u32 words[20];
} TerminalCommandPose;

/* Address view for every possible u8 index at offset606E. The256-byte
 * window expresses the address range, not an established logical table size. */
typedef struct TerminalExitByteView {
    u8 prefix[0x606E];
    u8 window606E[256];
} TerminalExitByteView;

typedef struct TerminalCommandPayload { u8 bytes[8]; } TerminalCommandPayload;
typedef struct TerminalCommandRecord TerminalCommandRecord;
typedef struct TerminalCommandChild {
    void *word00;
    int scaledX, scaledY, scaledZ;
    u8 gap10[8];
    void *word18;
    s16 currentX, currentY, currentZ;
    u8 gap22[6];
    s16 startX, startY, startZ;
    s16 half2E, half30, half32;
    s16 value34;
    TerminalCommandPayload payload;
    u8 gap3E[2];
    TerminalCommandRecord *owner;
    TerminalCommandPose pose;
    int phase;
} TerminalCommandChild;

struct TerminalCommandRecord {
    TerminalCommandChild *first[3], *second[3], *extra18, *extra1C;
    u16 half20, half22;
    u8 gap24[12];
    u8 byte30, byte31, byte32, byte33, byte34, byte35;
    u16 half36, half38, half3A, half3C;
    u8 gap3E[2];
    u32 flags;
    int word44, word48, word4C, word50, word54, word58;
    int word5C, word60, word64, word68, word6C, word70, word74;
    int word78, word7C;
    u8 gap80[24];
    int word98, word9C, wordA0;
    u8 byteA4, byteA5, byteA6, byteA7, byteA8, byteA9, byteAA, byteAB;
    int wordsAC[9];
    u8 byteD0, byteD1, bytesD2[12], bytesDE[12], bytesEA[12], byteF6, byteF7;
};

typedef struct TerminalCommandStream {
    u32 word00, word04, word08;
    u8 gap0C[0x804];
    u32 cursor, length;
    u8 gap818;
    u8 byte819;
    u8 gap81A[6];
    u8 byte820, byte821, byte822, byte823, byte824;
    u8 gap825[7];
    u8 byte82C, byte82D, byte82E;
} TerminalCommandStream;

typedef struct TerminalCommandVector { float x, y, z; } TerminalCommandVector;
typedef struct TerminalMotionState TerminalMotionState;
typedef struct TerminalMotionActor TerminalMotionActor;
typedef struct TerminalCopyState TerminalCopyState;

extern TerminalCommandStream *D_801CE8C0;
extern u8 *D_801CE8BC;
extern int D_801CE8C4;
extern u8 *D_801D0738;
extern u8 D_8021FEE0[];
extern u8 D_800EB290[];
extern u16 D_801CEF14, D_801CEF16;
extern u16 D_80196A70;
extern u8 D_801976DC, D_801976E8;
extern u8 D_801971F2[], D_80193BC0[], D_80195560[];
extern u8 D_801CEAAC, D_801CEAAD, D_801CEAAE, D_801CEAA9;

/* These function symbols are used only as opaque stored addresses here.
 * No call signature or callback behavior is inferred by these declarations. */
extern void func_001F7370();
extern void func_001F7148();
extern void func_001F7584();
extern void func_001F7828();

/* The retained ROM copy routine is source-first. */
extern void memcpy(void *source, void *destination, u32 size);
extern void func_80093380(void *, u32);
extern float vec3_distance(TerminalCommandVector *, TerminalCommandVector *);
extern void func_0004196c(int, int);
extern u16 func_00043e88(int, int), func_00043edc(int, int);
extern u16 func_00043f30(int, int), func_00043f84(int, int);
extern int func_00044238(int, int, int), func_00044340(int);
extern void func_00044aa4(void *), func_00045d78(int, int);
extern void func_000466f4(void), func_00048294(void);
/* Director2069 binds the compatible return view to a future combined source
 * correction; accepted 4813C remains void until that integration. */
extern int func_0004813C(int);
extern void func_001F0E04(void *, int);
extern void func_001F0E64(Func001F0E64Record *, u32);
extern void func_001F0F90(void *), func_001F1050(void *);
extern int func_001F197C(void *, Func001F197CState *, int *);
extern void func_001F2BC0(int, int), func_001F2F9C(void *, void *);
extern void func_001F3078(int);
extern void func_001FF868(s16, s16, s16, s16, u8);
extern u8 func_002014C4(int), func_00201584(int, int);
extern u8 func_00201838(int, int), func_002018A0(int, int);
extern void func_00208DC8(void *, int), func_0020BD5C(int);
extern int func_0020BF34(int), func_0020BF50(int);
extern int func_0020BF7C(void), func_0020BF8C(void), func_0020BFE4(void);
extern int func_0020BFF8(void *), func_0020C014(void *);
extern int func_0020C0CC(void *), func_0020C104(void *), func_0020C2C0(void *);
extern int func_0020C2F4(void *), func_0020C310(void *), func_0020C32C(void *);
extern int func_0020C348(void *);
extern void *func_0020C478(u32);
extern void func_0020C4B8(int, int, int, int *, int *);
extern u8 func_0020D434(int);
extern int func_0020D444(u16, u16), func_0020D4C8(void *, int);
extern int func_0020D72C(void *), func_0020F80C(void);
extern void func_0020FA04(void *, int), func_002158E4(void);
extern void func_0021AE6C(float, int), func_0021C074(void *), func_0021C140(void *);
extern u32 func_0021C970(u32);
extern void func_0021D784(void *), func_0021D7A8(void *), func_0021D7CC(void *);
extern void func_0021D7F0(TerminalMotionState *, int, int);
extern void func_0021DCA4(TerminalMotionState *, TerminalMotionActor *);
extern void func_0021E070(TerminalMotionState *, TerminalMotionActor *);
extern void func_0021E2C4(TerminalMotionState *, float, float, u8);
extern void func_0021E50C(TerminalMotionState *);
extern void func_0021E99C(TerminalCopyState *, TerminalCopyState *);
extern void *func_00224930(void *, int, int);
extern void func_00224D18(void *, u8 *), func_002267A8(void *, int, int);
extern void func_0022EF50(void *), func_0025FC94(int, u8 *, u8 *, u8 *);
extern void func_0025FD90(s16, s16 *, s16 *);

#define BYTE_AT(base, offset) (((u8 *)(base))[offset])
#define WORD_AT(base, offset) (*(u32 *)((u8 *)(base) + (offset)))
#define SHORT_AT(base, offset) (*(u16 *)((u8 *)(base) + (offset)))
#define POSE(child) (&(child)->pose.view.record)
#define SELECTED_CHILDREN() \
    for (i = 0; i < 3u; i++) \
        if (((record->byteA5 >> i) & 1) != 0 && record->first[i] != 0)
#define COPY_TRIPLE(child, selector) \
    (child)->half2E = D_801D0738[(selector) * 3]; \
    (child)->half30 = D_801D0738[(selector) * 3 + 1]; \
    (child)->half32 = D_801D0738[(selector) * 3 + 2]
#define ACCOUNT_VALUE() \
    if (func_0020C014(record) != 0) func_0021AE6C(0.0f, value); \
    if (func_0020C014(record) != 0) WORD_AT(D_801CE8BC, 0x6060) += value; \
    else WORD_AT(D_801CE8BC, 0x6064) += value
#define CLEAR_PAIRS() \
    SELECTED_CHILDREN() { \
        func_001F0F90(record->first[i]); \
        func_001F1050(record->second[i]); \
        record->first[i]->word18 = 0; \
        record->second[i]->word18 = 0; \
        record->first[i] = 0; record->second[i] = 0; \
    }

/* Give each command its own inline expansion before the compiler
 * merges equivalent extra-record updates in its late jump pass. */
static inline void update_extras(TerminalCommandRecord *record)
{
    TerminalCommandChild *child;
    int selector;
    child = record->extra18;
    if (child != 0) {
        selector = (s16)child->pose.view.record.field_04;
        if (selector != 0 && ((u32)(selector - 1) & 3u) < 2u)
            func_001F0E64(POSE(child), (u32)selector + 2);
    }
    {
        TerminalCommandChild *second = record->extra1C;
        if (second != 0) {
            selector = (s16)second->pose.view.record.field_04;
            if (selector != 0 && ((u32)(selector - 1) & 3u) < 2u)
                func_001F0E64(POSE(second), (u32)selector + 2);
        }
    }
}

void func_0021EBBC(void)
{
    s16 interpolationOutput[4];
    s16 interpolationInput[9];
    TerminalCommandVector firstVector, secondVector;
    TerminalCommandRecord firstCopy, secondCopy;
    struct { int x, y; } firstOutput;
    TerminalCommandPose copiedPose;
    int secondOutputX, secondOutputY;
    int exitFlag;
    TerminalCommandRecord *record;
    TerminalCommandStream *waitStream;
    u8 *command;
    int raw;
    u32 waitLimit;
    int i;

    exitFlag = 0;
    for (;;) {
        command = (u8 *)D_801CE8C0 + (D_801CE8C0->cursor + 0x10u);
        waitLimit = command[1];
        raw = command[0];
        if (((u8 *)D_801CE8C0 + D_801CE8C0->cursor)[0x12] == D_801CE8C0->word04) {
        D_801CE8C0->word04 = 0;
        switch (raw) {
        case 0x41: /* 21EC4C */
            D_801CE8C4 = 0;
            exitFlag = 1;
            goto exit_checks;
        case 0x40: /* 21EC60 -> 21F268 */
            waitStream = D_801CE8C0;
            goto wait_command;
        case 0x16: { /* 21EC6C */
            u8 *firstEnd, *secondEnd, *scan;
            int slot;
            TerminalCommandRecord *source;
            int firstSide, mode, flag;
            u32 childIndex;
            firstEnd = command + 5;
            while (*firstEnd != 255) firstEnd++;
            secondEnd = firstEnd + 1;
            while (*secondEnd != 255) secondEnd++;
            D_801CE8C0->byte821 = command[5];
            D_801CE8C0->byte822 = 1;
            D_801CE8C0->byte820 = command[4];
            source = func_0020C478(D_801CE8C0->byte821);
            memcpy(command + 5, source->bytesD2, 12);
            memcpy(firstEnd + 1, source->bytesDE, 12);
            memcpy(secondEnd + 1, source->bytesEA, 12);
            if ((u32)(D_801CE8C0->byte820 - 0x2F) < 4u) {
                int count = 1;
                u32 sourceIndex;
                for (sourceIndex = 0; sourceIndex < 20u; sourceIndex++) {
                    TerminalCommandRecord *other;
                    other = func_0020C478(sourceIndex);
                    if (func_0020C2C0(other) != 0 || other == source) continue;
                    firstSide = func_0020C014(other);
                    if (firstSide == func_0020C014(source)) {
                        source->bytesD2[count] = func_0020D72C(other);
                        count++;
                    }
                }
                /* This unsigned identity equals count. It retains a real
                 * firstEnd reference until the compiler folds the expression. */
                source->bytesD2[
                    ((u32)count | (u32)firstEnd) & (u32)count] = 255;
            }
            if (source->bytesDE[0] == 255) {
                source->bytesDE[0] = func_0020C014(source) ? 0x18 : 0x2A;
                source->bytesDE[1] = 255;
            }
            func_0025FC94(command[4], source->bytesD2, source->bytesDE, source->bytesEA);
            D_801CE8C0->byte824 = 0;
            firstEnd++;
            for (;;) {
                if (*firstEnd != 255) {
                    if (*firstEnd < 20) D_801CE8C0->byte824++;
                    firstEnd++;
                } else {
                    break;
                }
            }
            switch (D_801CE8C0->byte820) {
            case 0x61:
            case 0x66:
            case 0x6B:
                D_801CE8C0->byte824++;
                break;
            default:
                break;
            }
            D_801CE8C0->byte823++;
            for (i = 0; i < 20u; i++) {
                    TerminalCommandRecord *other;
                other = func_0020C478(i);
                if (func_0020C2C0(other) == 0) {
                    other->byteD0 = 0; other->byteD1 = 0;
                }
            }
            if (func_0020BFE4() != 0) {
                for (i = 0, flag = 0; i < 20u; i++, flag = 0) {
                    TerminalCommandRecord *other;
                    TerminalCommandChild *child;
                    other = func_0020C478(i);
                    if (func_0020C014(other) != 0) {
                        int skipMode;
                        mode = D_801CE8C0->byte820;
                        skipMode = ((mode == 7) | (mode == 15)) ||
                            ((mode == 23) | (mode == 31)) ||
                            ((mode == 38) | (mode == 46));
                        if (skipMode != 0) continue;
                    }
                    if (func_0020C2C0(other) != 0) continue;
                    if (source->word48 == 0x87 && func_0020C0CC(other) != 0) flag = 1;
                    for (slot = 0; source->bytesD2[slot] != 255; slot++)
                        if (source->bytesD2[slot] == i) { flag = 1; break; }
                    for (slot = 0; source->bytesDE[slot] != 255; slot++)
                        if (source->bytesDE[slot] == i) { flag = 2; break; }
                    for (childIndex = 0; childIndex < 3u; childIndex++) {
                        TerminalCommandPayload *payload;
                        child = other->first[childIndex];
                        if (child == 0) continue;
                        payload = &child->payload;
                        interpolationInput[0] = child->payload.bytes[1];
                        interpolationInput[1] = child->payload.bytes[2];
                        interpolationInput[2] = child->payload.bytes[3];
                        interpolationInput[3] = child->payload.bytes[4];
                        interpolationInput[5] = child->payload.bytes[5];
                        interpolationInput[6] = child->payload.bytes[6];
                        interpolationInput[7] = child->payload.bytes[7];
                        /* Input [4]/[8] are not initialized by this caller. */
                        func_0025FD90(payload->bytes[0], interpolationInput, interpolationOutput);
                        child->payload.bytes[5] = (u16)interpolationOutput[0];
                        child->payload.bytes[6] = (u16)interpolationOutput[1];
                        child->payload.bytes[7] = (u16)interpolationOutput[2];
                        if (flag != 0) {
                            child->payload.bytes[4] = 255;
                            child->payload.bytes[3] = 255;
                            child->payload.bytes[2] = 255;
                        } else {
                            int payloadOffset;
                            payloadOffset = D_801CE8C0->byte820 * 3;
                            child->payload.bytes[2] = D_8021FEE0[payloadOffset];
                            payloadOffset = D_801CE8C0->byte820 * 3;
                            child->payload.bytes[3] = D_8021FEE0[payloadOffset + 1];
                            payloadOffset = D_801CE8C0->byte820 * 3;
                            child->payload.bytes[4] = D_8021FEE0[payloadOffset + 2];
                        }
                        payload->bytes[0] = 0;
                        payload->bytes[1] = 10;
                    }
                    if (other->extra18 != 0) other->extra18->payload = other->first[0]->payload;
                    if (other->extra1C != 0) other->extra1C->payload = other->first[0]->payload;
                }
            }
            goto advance_command;
        }
        case 0x17: { /* 21F174 */
            TerminalCommandRecord *source = func_0020C478(command[5]);
            memcpy(command + 5, source->bytesD2, 12);
            memcpy(command + 7, source->bytesDE, 12);
            func_0025FC94(command[3], source->bytesD2, source->bytesDE, 0);
            goto advance_command;
        }
        case 0x20: /* 21F1C4 */
            if (D_801CE8C0->byte822 != 0) goto exit_checks;
            goto advance_command;
        case 0x1E:
        case 0x38:
            goto advance_command;
        default:
            break;
        }

        record = func_0020C478(command[3]);
        waitLimit = command[1];
        if (record != 0) {
            if (raw != 0x33) {
            record->byteA5 = command[4]; record->byteA6 = command[5];
            record->byteA7 = command[6]; record->byteA8 = command[7];
            record->byteA9 = command[8]; record->byteAA = command[9];
            }
        }
        switch (raw) {
        case 0x1C: /* 21F254 */
            record->word6C = (u32)record->word6C + 1u;
            waitStream = D_801CE8C0;
wait_command:
            if (waitStream->byte819 < waitLimit) waitStream->byte819 = waitLimit;
            break;
        case 1: { /* 21F280: all three slots, no mask/null guard */
            u32 index;
            for (index = 0; index < 3u; index++) func_001F0E04(POSE(record->first[index]), 0);
            break;
        }
        case 2: { /* 21F2B4 */
            TerminalCommandChild *child;
            TerminalCommandRecord *target;
            u8 sourceAdjustment, targetAdjustment;
            int adjustment, currentX;
            float distance, shortened, xIncrement, zIncrement;
            SELECTED_CHILDREN() {
                record->word68 = record->byteA6;
                target = func_0020C478(record->byteA6);
                child = record->first[i];
                sourceAdjustment = func_00201838(child->owner->word48, child->owner->word4C);
                targetAdjustment = func_002018A0(target->word48, target->word4C);
                firstVector.x = child->currentX; firstVector.y = 0.0f;
                firstVector.z = child->currentZ;
                adjustment = (int)sourceAdjustment + (int)targetAdjustment;
                secondVector.x = func_0020BF50(target->word58); secondVector.y = 0.0f;
                secondVector.z = func_0020BF34(target->word54);
                distance = vec3_distance(&firstVector, &secondVector);
                currentX = child->currentX;
                shortened = distance - (float)adjustment;
                xIncrement = shortened * (float)(int)((u32)target->word5C - currentX) / distance;
                zIncrement = shortened * (float)(int)((u32)target->word64 - child->currentZ) / distance;
                func_0021D7F0((TerminalMotionState *)child,
                    (int)((float)currentX + xIncrement),
                    (int)((float)child->currentZ + zIncrement));
                func_001F0E64(POSE(record->first[i]), record->byteA7);
            }
            record->word74 = 2;
            break;
        }
        case 3: /* 21F440 */
            SELECTED_CHILDREN() {
                TerminalCommandRecord *target;
                record->word68 = record->byteA6;
                target = func_0020C478(record->byteA6);
                func_0021E070((TerminalMotionState *)record->first[i], (TerminalMotionActor *)target);
                func_001F0E64(POSE(record->first[i]), record->byteA7);
            }
            record->word74 = 2;
            break;
        case 4: { /* 21F4AC */
            TerminalCommandChild *child;
            TerminalCommandRecord *target;
            int deltaX, deltaZ, x, z, travel;
            SELECTED_CHILDREN() {
                record->word68 = record->byteA6;
                target = func_0020C478(record->byteA6);
                child = record->first[i];
                deltaX = (u32)target->word5C - child->owner->word5C;
                if (deltaX <= 0) deltaX = (u32)child->owner->word5C - target->word5C;
                deltaZ = (u32)target->word64 - child->owner->word64;
                if (deltaZ <= 0) deltaZ = (u32)child->owner->word64 - target->word64;
                x = func_0020BF50(func_0020C014(child->owner) ? 5 : 3);
                if ((int)((u32)x - child->owner->word5C) > 0) {
                    travel = (u32)x - child->owner->word5C;
                } else {
                    travel = (u32)child->owner->word5C - x;
                }
                /* This single-pass C loop retains KMC's loop notes around
                 * the quotient. They keep the following loads after the divide
                 * and preserve the retail allocation without extra operations. */
                do {
                    z = (int)((u32)deltaZ * travel) / deltaX;
                } while (0);
                {
                    int ownerZ = child->owner->word64;
                    z = target->word54 < child->owner->word54
                        ? (u32)ownerZ - z : (u32)z + ownerZ;
                }
                func_0021D7F0((TerminalMotionState *)child, x, z);
                func_001F0E64(POSE(record->first[i]), record->byteA7);
            }
            record->word74 = 2;
            break;
        }
        case 5: /* 21F5E0 */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), record->byteA6);
            record->word74 = 5;
            break;
        case 6: /* 21F630 */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), record->byteA6);
            record->word74 = 6;
            if (func_0020BFE4() == 0) {
                memcpy(record, &firstCopy, 248);
                func_0022EF50(&firstCopy);
                func_002267A8(record, 14, firstCopy.word7C);
            }
            break;
        case 7: /* 21F6AC */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), record->byteA6);
            record->word74 = 6;
            if (func_0020BFE4() == 0) {
                memcpy(record, &secondCopy, 248);
                func_0022EF50(&secondCopy);
                func_002267A8(record, 14, secondCopy.word7C);
            }
            break;
        case 0x21: { /* 21F734 */
            TerminalCommandRecord *target = func_0020C478(record->byteA7);
            func_0021DCA4((TerminalMotionState *)record->first[0], (TerminalMotionActor *)target);
            func_001F0E64(POSE(record->first[0]), record->byteA6);
            record->word74 = 0x21;
            break;
        }
        case 8: /* 21F764 */
            SELECTED_CHILDREN() {
                func_0021E50C((TerminalMotionState *)record->first[i]);
                func_001F0E64(POSE(record->first[i]), record->byteA6);
            }
            record->word68 = -1; record->word74 = 8;
            break;
        case 9: /* 21F7C4 */
            SELECTED_CHILDREN() {
                func_001F0E64(POSE(record->first[i]), record->byteA6);
                func_0021E99C((TerminalCopyState *)record->first[i], (TerminalCopyState *)record->second[i]);
            }
            record->word74 = 1;
            break;
        case 10: /* 21F81C */
            SELECTED_CHILDREN() {
                func_0021E50C((TerminalMotionState *)record->first[i]);
                func_001F0E64(POSE(record->first[i]), record->byteA6);
                record->first[i]->phase = record->byteA7;
            }
            record->word68 = -1; record->word74 = 10;
            break;
        case 11: { /* 21F888 */
            int effect, side;
            if (record->byteA8 != 0)
                SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), record->byteA8);
            if (func_0020BFE4() == 0)
                func_002267A8(record, 14, func_002014C4(record->byteA7) & 255);
            effect = func_00044340(func_002014C4(record->byteA7) & 255);
            side = func_0020C014(record);
            func_001F2BC0(effect, side);
            break;
        }
        case 12: { /* 21F934 */
            int value = (record->byteA6 << 8) + record->byteA7;
            record->wordA0 = 10;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                if (func_0020C32C(record) == 0) {
                    func_001F0E64(POSE(child), 17);
                } else {
                    child->phase = 0;
                }
                COPY_TRIPLE(child, record->byteA8);
            }
            if (value != 65535) {
                func_002267A8(record, 1, value);
                if (value >= record->half20) { value = record->half20; record->half20 = 0; }
                else record->half20 -= value;
                ACCOUNT_VALUE();
            }
            if (func_0020C32C(record) != 0)
                func_00208DC8(D_800EB290, record->word98 < 160 ? D_801CEF14 : D_801CEF16);
            record->word74 = 12;
            break;
        }
        case 0x32: { /* 21FAD8: distinct body and coordinate outputs */
            int value = (record->byteA6 << 8) + record->byteA7;
            record->wordA0 = 10;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                if (func_0020C32C(record) == 0) {
                    func_001F0E64(POSE(child), 17);
                } else {
                    child->phase = 0;
                }
                COPY_TRIPLE(child, record->byteA8);
            }
            firstOutput.x = func_0020C014(record) ? (u32)record->word5C + 38 : (u32)record->word5C - 38;
            func_0020C4B8(firstOutput.x, record->word60, record->word64, &firstOutput.x, &firstOutput.y);
            {
                int x = firstOutput.x, y = firstOutput.y;
                record->word98 = x;
                record->word9C = y;
            }
            if (value != 65535) {
                func_002267A8(record, 1, value);
                if (value >= record->half20) { value = record->half20; record->half20 = 0; }
                else record->half20 -= value;
                ACCOUNT_VALUE();
            }
            if (func_0020C32C(record) != 0)
                func_00208DC8(D_800EB290, record->word98 < 160 ? D_801CEF14 : D_801CEF16);
            record->word74 = 0x32;
            break;
        }
        case 0x2D: /* 21FCD0 */
            SELECTED_CHILDREN() record->first[i]->phase = 0;
            record->word74 = 0x2D;
            func_00208DC8(D_800EB290, record->word98 < 160 ? D_801CEF14 : D_801CEF16);
            break;
        case 13: /* 21FD44 */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), 18);
            record->word74 = 1;
            break;
        case 14: /* 21FD90 */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                u32 absent = (func_00201584(record->word48, record->word4C) & 255) == 0;
                func_001F0E64(POSE(child), ((0u - absent) & 23) | 21);
            }
            func_002267A8(record, 3, 0);
            break;
        case 15: /* 21FDF8 */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), (func_00201584(record->word48, record->word4C) & 255) ? 22 : 24);
            }
            record->word74 = 1;
            break;
        case 16: { /* 21FE5C */
            int value = (record->byteA6 << 8) + record->byteA7;
            record->wordA0 = 10;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), 25);
                COPY_TRIPLE(child, record->byteA8);
            }
            func_002267A8(record, 7, value);
            if (value >= record->half20) { value = record->half20; record->half20 = 0; }
            else record->half20 -= value;
            ACCOUNT_VALUE();
            break;
        }
        case 17: /* 21FF9C */
            record->wordA0 = 10;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), 25);
                COPY_TRIPLE(child, record->byteA6);
            }
            func_002267A8(record, 13, 0);
            break;
        case 18: /* 220044 */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), 26);
            record->word74 = 1;
            break;
        case 19:
        case 0x30: { /* 220090 */
            int divisor;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), 29);
                copiedPose = child->pose; /* complete 80-byte 44..94 extent */
                divisor = func_001F197C(record, &copiedPose.calculation.state, 0);
                if (divisor == 0) divisor = 1;
                child->phase = 255 / divisor;
            }
            record->word74 = 19;
            if (func_0020C104(record) == 0 || func_0020D444(record->half20, record->half22) == 0) {
                TerminalCommandChild *child;
                int selector;
                child = record->extra18;
                if (child != 0) {
                    selector = (s16)child->pose.view.record.field_04;
                    if (selector != 0 && ((u32)(selector - 1) & 3u) < 2u)
                        func_001F0E64(POSE(child), (u32)selector + 2);
                }
                child = record->extra1C;
                if (child != 0) {
                    selector = (s16)child->pose.view.record.field_04;
                    if (selector != 0 && ((u32)(selector - 1) & 3u) < 2u)
                        func_001F0E64(POSE(child), (u32)selector + 2);
                }
            }
            if (raw == 0x30) func_002267A8(record, 18, 0);
            break;
        }
        case 20:
        case 0x31: { /* 2201F8 */
            int count, side, event, key;
            u32 lookupIndex;
            u8 *scan, *end;
            CLEAR_PAIRS();
            record->half20 = 0;
            record->flags = (record->flags & ~0x3Eu) | 1;
            if (raw != 0x31 || func_0020BF7C() != 0 || func_0020BF8C() != 0) break;
            if (func_0020C014(record) != 0) {
                u16 *counter = &D_80196A70;
                int counterValue = *counter + 1;
                if (counterValue >= 10000) counterValue = 9999;
                *counter = counterValue;
            } else {
                u8 *state = D_801CE8BC;
                int counterIndex = BYTE_AT(state, 0x608D);
                BYTE_AT(state, 0x608D) = counterIndex + 1;
                BYTE_AT(state, 0x608E + counterIndex) = record->byte31;
            }
            switch (record->word48) {
            case 0x58: event = 0xA4; break;
            case 0x5A: event = 0xA5; break;
            case 0x59: event = 0xA6; break;
            case 0x5B: event = 0xA7; break;
            case 0x5C: event = 0xA8; break;
            default: goto after_record_dispatch;
            }
            func_00045d78(event, 1);
after_record_dispatch:
            func_0004196c(func_0020BFF8(record) & 255, record->byteF6);
            side = func_0020C014(record);
            /* Keep the common reset separate from the per-arm write that
             * must precede the selector byte read in this compiler. */
            record->word4C = 0;
            if (side != 0) {
                *(volatile int *)&record->word48 = 0;
                count = D_801976DC;
                key = record->byteF6;
            } else {
                *(volatile int *)&record->word48 = 0;
                count = D_801976E8;
                key = record->byteF6;
            }
            {
                scan = D_801971F2 + count * 25;
                for (lookupIndex = 0; lookupIndex < 5; lookupIndex++) {
                    if (scan[lookupIndex] == key) {
                        scan[lookupIndex] = 0;
                        break;
                    }
                }
            }
            break;
        }
        case 21: /* 2203CC */
            CLEAR_PAIRS();
            record->word74 = 1;
            break;
        case 0x18: { /* 220434 */
            int value = (record->byteA6 << 8) + record->byteA7;
            SELECTED_CHILDREN() record->first[i]->word00 = (void *)func_001F7370;
            record->half20 += func_0020D4C8(record, value);
            func_002267A8(record, 2, value);
            break;
        }
        case 0x19: /* 2204B4 */
            SELECTED_CHILDREN() record->first[i]->word00 = (void *)func_001F7148;
            break;
        case 0x1A: /* 2204FC */
            record->wordA0 = 10;
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                if (func_0020C32C(record) == 0) func_001F0E64(POSE(child), 32);
                COPY_TRIPLE(child, record->byteA7);
            }
            record->flags |= 8;
            record->word74 = 0x1A;
            func_002267A8(record, 6, 0);
            if (func_0020C32C(record) == 0) func_0020FA04(record, 9);
            break;
        case 0x1B: { /* 2205E8 */
            u32 clearedFlags;
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), 33);
            clearedFlags = record->flags & ~8u;
            record->word74 = 1;
            record->flags = clearedFlags;
            func_002267A8(record, 11, 0);
            update_extras(record);
            break;
        }
        case 0x23: /* 22065C */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                child->word00 = (void *)func_001F7584; child->phase = 0;
                func_001F0E64(POSE(child), 38);
            }
            record->flags |= 2; record->word74 = 0x23;
            func_002267A8(record, 4, 0);
            update_extras(record);
            break;
        case 0x24: /* 2206DC */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                child->word00 = (void *)func_001F7828; child->phase = 0;
                func_001F0E64(POSE(child), 39);
                child->pose.view.record.field_08 = 3;
            }
            record->flags &= ~2u; record->word74 = 0x24;
            func_002267A8(record, 11, 0);
            break;
        case 0x25: /* 220768 */
            SELECTED_CHILDREN() {
                if (func_0020C32C(record) == 0 && func_0020C2F4(record) == 0)
                    func_001F0E64(POSE(record->first[i]), 34);
            }
            record->flags |= 4; record->word74 = 0x25;
            func_002267A8(record, 5, 0);
            if (func_0020C32C(record) == 0 && func_0020C2F4(record) == 0)
                func_0020FA04(record, 5);
            break;
        case 0x26: { /* 220824 */
            u32 clearedFlags;
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), 35);
            clearedFlags = record->flags & ~4u;
            record->word74 = 1;
            record->flags = clearedFlags;
            func_002267A8(record, 11, 0);
            update_extras(record);
            break;
        }
        case 0x27: /* 220898 */
            record->flags |= 16;
            func_0021D7CC(record);
            func_002267A8(record, 8, 0);
            if (func_0020C32C(record) == 0 && func_0020C2F4(record) == 0 && func_0020C310(record) == 0)
                func_0020FA04(record, 1);
            break;
        case 0x28: /* 2208FC */
            record->flags &= ~16u;
            func_0021D7CC(record);
            func_002267A8(record, 11, 0);
            update_extras(record);
            break;
        case 0x29: /* 22092C */
            func_0021D784(record);
            func_002267A8(record, 9, 0);
            break;
        case 0x2A: /* 220940 */
            func_0021D7A8(record);
            func_002267A8(record, 10, 0);
            break;
        case 0x2B: /* 220960 */
            record->flags &= ~0x3Eu;
            func_0021D7CC(record);
            update_extras(record);
            break;
        case 0x3F: /* 2209E0 */
            record->byte34 += record->byteA5;
            break;
        case 0x35: /* 2209F4 */
            record->byte32 += record->byteA5;
            break;
        case 0x36: /* 220A08 */
            func_002267A8(record, record->byteA5, (record->byteA6 << 8) | record->byteA7);
            break;
        case 0x37: /* 220A2C */
            func_002267A8(record, 12, 0);
            record->byte32 = 0;
            func_0021C140(record);
            func_00044aa4(D_80193BC0 + record->byteF6 * 56);
            func_0021C074(record);
            break;
        case 0x2E: /* 220A78 */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), 5); child->phase = 10;
            }
            record->word74 = 0x2E;
            break;
        case 0x2F: /* 220AD0 */
            SELECTED_CHILDREN() {
                TerminalCommandChild *child = record->first[i];
                func_001F0E64(POSE(child), 11); child->phase = 10;
            }
            record->word74 = 0x2F;
            break;
        case 0x33: /* 220B28 */
            func_0020C4B8(record->word5C, record->word60, record->word64, &secondOutputX, &secondOutputY);
            secondOutputX = (u32)secondOutputX - 160;
            secondOutputY = (u32)secondOutputY - 120;
            func_001FF868((s16)(0u - secondOutputX), (s16)(0u - secondOutputY), 128, 128, 2);
            D_801CE8C0->byte82D = 24;
            break;
        case 0x34: { /* 220B9C */
            int finalComponent;
            D_801CEAAC = D_801D0738[record->byteA6 * 3];
            D_801CEAAD = D_801D0738[record->byteA6 * 3 + 1];
            finalComponent = D_801D0738[record->byteA6 * 3 + 2];
            D_801CEAA9 = 128;
            D_801CEAAE = finalComponent;
            D_801CE8C0->byte82C = 4;
            break;
        }
        case 0x1F: { /* 220C18 */
            for (i = 7; i >= 0; i--)
                record->wordsAC[i + 1] = record->wordsAC[i];
            record->wordsAC[1] = D_801CE8C0->cursor;
            if (record->byteD0 != 0) record->byteD1 = 1;
            record->byteD0++;
            break;
        }
        case 0x1D: /* 220C64 */
            SELECTED_CHILDREN() func_001F0E64(POSE(record->first[i]), record->byteA6);
            break;
        case 0x39: { /* 220CB0 */
            TerminalCommandRecord *other;
            int mode;
            record->flags |= 0x800;
            other = func_00224930(record, record->byteA5, record->byteA6);
            record->first[0]->phase = 8;
            {
                TerminalCommandChild *second = other->second[0];
                TerminalCommandChild *first = other->first[0];
                second->value34 = 0;
                first->value34 = 0;
            }
            other->first[0]->phase = record->first[0]->phase;
            mode = 0;
            if (func_0020C2F4(record) != 0) mode = 9;
            else if (func_0020C2F4(record) == 0 && func_0020C310(record) != 0) mode = 5;
            else if (func_0020C2F4(record) == 0 && func_0020C310(record) == 0 && func_0020C348(record) != 0) mode = 1;
            mode &= 255;
            if (mode != 0) {
                func_001F0E64(POSE(other->first[0]), (s16)record->first[0]->pose.view.record.field_04);
                func_0020FA04(other, mode);
            }
            record->word74 = 0x39; other->word74 = 1;
            func_001F2F9C(record, other);
            break;
        }
        case 0x3A: { /* 220DC0 */
            int destinationIndex, sourceIndex;
            u8 *unit;
            destinationIndex = func_0020D72C(record);
            sourceIndex = record->byteA5;
            func_001F0F90(record->first[0]); func_001F1050(record->second[0]);
            record->first[0]->word18 = 0; record->second[0]->word18 = 0;
            record->first[0] = 0; record->second[0] = 0;
            if (record->extra18 != 0) record->extra18->word18 = 0;
            if (record->extra1C != 0) record->extra1C->word18 = 0;
            memcpy(D_801CE8BC + 0x1C4 + sourceIndex * 248,
                   D_801CE8BC + 0x1C4 + destinationIndex * 248, 248);
            func_80093380(D_801CE8BC + 0x1C4 + sourceIndex * 248, 248);
            {
                TerminalCommandChild *second = record->second[0];
                TerminalCommandChild *first = record->first[0];
                second->owner = record;
                first->owner = record;
            }
            if (record->extra18 != 0) record->extra18->owner = record;
            if (record->extra1C != 0) record->extra1C->owner = record;
            if (func_0020C014(record) != 0) {
                if (D_801976DC < 30) unit = D_80193BC0 + record->byteF6 * 56;
                else unit = D_80195560 + record->byteF6 * 52;
            } else {
                if (D_801976E8 < 30) unit = D_80193BC0 + record->byteF6 * 56;
                else unit = D_80195560 + record->byteF6 * 52;
            }
            {
                int secondValue;
                unit[0x11] = record->word48;
                secondValue = record->word4C;
                SHORT_AT(unit, 0x30) = 0; SHORT_AT(unit, 0x2E) = 0;
                SHORT_AT(unit, 0x2C) = 0; SHORT_AT(unit, 0x2A) = 0;
                unit[0x12] = secondValue;
            }
            record->half36 = func_00043e88(BYTE_AT(record, 0x4B), BYTE_AT(record, 0x4F));
            record->half38 = func_00043edc(BYTE_AT(record, 0x4B), BYTE_AT(record, 0x4F));
            record->half3A = func_00043f30(BYTE_AT(record, 0x4B), BYTE_AT(record, 0x4F));
            record->half3C = func_00043f84(BYTE_AT(record, 0x4B), BYTE_AT(record, 0x4F));
            if (record->flags & 0x800) record->flags &= ~0x800u;
            if (record->flags & 0x2000) record->flags &= ~0x2000u;
            break;
        }
        case 0x3B: /* 220FB0: crosses original physical-part boundary */
            if (record == 0) {
                if (D_801CE8C0->byte819 < waitLimit) D_801CE8C0->byte819 = waitLimit;
                record = (TerminalCommandRecord *)(D_801CE8BC + 0x1C4 + command[3] * 248);
                func_00224D18(record, command + 3);
                D_801CE8C0->byte82E |= record->byteA7 ? 2 : 1;
                for (i = 0; i < 3u; i++) {
                    /* Original tests bit 1 for every slot, not bit i. */
                    if (((record->byteA5 >> 1) & 1) != 0 && record->first[i] != 0) {
                        TerminalCommandChild *second, *first;
                        record->first[i]->phase = 17;
                        second = record->second[i];
                        first = record->first[i];
                        second->value34 = 0;
                        first->value34 = 0;
                    }
                }
                record->word74 = 0x3B; record->byteA6 = 0;
            }
            func_001F3078(func_0020C014(record));
            break;
        case 0x3C: { /* 22108C */
            float x, z;
            x = func_0020BF50((func_0020C014(record) == 0) << 3);
            z = record->word64;
            SELECTED_CHILDREN() func_0021E2C4((TerminalMotionState *)record->first[i], x, z, 15);
            record->byteA6 = 0; record->word74 = 0x3C;
            break;
        }
        case 0x3D: { /* 221110 */
            TerminalCommandRecord *other;
            CLEAR_PAIRS();
            for (i = 0; i < 20; i++) {
                other = func_0020C478(i);
                if (other != 0 && (other->flags & 0x800)) other->flags &= ~0x800u;
            }
            func_80093380(D_801CE8BC + 0x1C4 + command[3] * 248, 248);
            break;
        }
        case 0x3E: /* 2211D8 */
            record->byteA4 = record->byteA5;
            break;
        default:
            break;
        }
        goto advance_command;
advance_command:
        {
            u32 next = func_0021C970(D_801CE8C0->cursor);
            D_801CE8C0->cursor = next;
        }
        } else {
            break;
        }
    }
    D_801CE8C0->word04++;
    D_801CE8C0->word08++;
exit_checks:
    if (exitFlag != 0) {
        int index;
        if (BYTE_AT(D_801CE8BC, 0x6044) == 1 &&
            ((TerminalExitByteView *)D_801CE8BC)->window606E[BYTE_AT(D_801CE8BC, 0x6072)] == 2) return;
        func_00048294();
        i = func_0004813C(16);
        func_000466f4();
        if ((func_0020BF7C() != 0 || func_0020BF8C() != 0) &&
            func_0020F80C() == 0 && i == 0 && BYTE_AT(D_801CE8BC, 0x6087) == 0) {
            for (index = 0; index < 20; index++) {
                TerminalCommandRecord *other = func_0020C478(index);
                if (func_0020C2C0(other) == 0) {
                    int third;
                    other->word6C = 0;
                    third = func_0020D434(other->word58) & 255;
                    other->word70 = func_00044238(BYTE_AT(other, 0x4B), BYTE_AT(other, 0x4F), third) & 255;
                }
            }
            D_801CE8C4 = 1;
            func_002158E4();
            if (BYTE_AT(D_801CE8BC, 0x6044) == 1 &&
                ((TerminalExitByteView *)D_801CE8BC)->window606E[BYTE_AT(D_801CE8BC, 0x6072)] == 9) {
                D_801CE8C4 = 0;
                ((TerminalExitByteView *)D_801CE8BC)->window606E[BYTE_AT(D_801CE8BC, 0x6072)] = 2;
            }
        } else if (BYTE_AT(D_801CE8BC, 0x6044) == 1 &&
                   ((TerminalExitByteView *)D_801CE8BC)->window606E[BYTE_AT(D_801CE8BC, 0x6072)] < 2) {
            if (BYTE_AT(D_801CE8BC, 0x6088) != 0) {
                func_0020BD5C(11);
            } else {
                func_0020BD5C(9);
            }
        }
    }
}
