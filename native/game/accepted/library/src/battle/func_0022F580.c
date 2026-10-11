typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;

typedef struct ResolutionObject {
    u8 *nested;
    u8 unknown04[0x1C];
    u16 field20;
    u8 unknown22[0x12];
    u8 field34;
    u8 unknown35[0xB];
    u32 field40;
    u8 unknown44[4];
    int field48;
    int field4C;
    u8 unknown50[4];
    int field54;
    int field58;
    u8 unknown5C[0xC];
    int field68;
    int field6C;
    int field70;
    u8 unknown74[4];
    u8 field78;
    u8 unknown79[3];
    u32 field7C;
    int field80;
    int field84;
    int field88;
    int field8C;
    int field90;
    int field94;
    u8 unknown98[0x60];
} ResolutionObject;

typedef struct ResolutionContext {
    int field00;
    u8 unknown04[0x814];
    u8 field818;
    u8 unknown819[0xA];
    u8 field823;
    u8 unknown824[4];
    int field828;
} ResolutionContext;

typedef struct ResolutionDescriptor {
    u8 header[9];
    u8 unknown09[3];
    int count;
    int ids[10];
    u16 halfwords[10];
    u8 primary[10];
    u8 bytes[10];
    s16 values[10];
    u8 secondary[10];
    u8 unknown7E[2];
} ResolutionDescriptor;

typedef struct ResolutionPointers { ResolutionObject *values[3]; } ResolutionPointers;
typedef struct StateBlock { int words[4]; } StateBlock;

typedef struct ResolutionRecord { u8 bytes[5]; } ResolutionRecord;
typedef struct InputRecord { u8 bytes[16]; } InputRecord;
typedef struct InitialRecord { u8 bytes[25]; } InitialRecord;

/* This overlay retains the observed adjacent work/list storage. The evaluator
   clears six words but its terminated output has no established universal bound.
   The word view preserves writes into subsequent observed work fields. */
typedef union ResolutionTail {
    int words[50];
    struct {
        int evaluator[6];
        int selectedTimes[3];
        u8 unknown24[4];
        u8 mode;
        u8 unknown29[7];
        ResolutionPointers temporaryList;
        u8 unknown3C[4];
        int out0;
        int out1;
        int out2;
        int out3;
        u8 modifier;
        u8 unknown51[7];
        int selectedIds[4];
        int ids[12];
        int flags[12];
    } fields;
} ResolutionTail;

typedef struct ResolutionScratch {
    u8 raw[3];
    u8 unknown03[5];
    ResolutionPointers selectedList;
    u8 unknown14[4];
    ResolutionRecord records[3];
    u8 unknown27;
    ResolutionDescriptor descriptor;
    StateBlock copiedState[5];
    ResolutionTail tail;
} ResolutionScratch;

extern ResolutionContext *D_801CE8C0;
extern InputRecord D_8018AA80[];
extern const InitialRecord D_801971F0[];
extern u8 D_801976DC;
extern u8 D_801976E8;
extern const ResolutionPointers D_801F6C74;
extern void func_00023780(void *, u32);
extern void func_00023460(const void *, void *, u32);
extern int func_00022e60(void);
extern u32 rand(void);
extern int func_0020C014(ResolutionObject *);
extern int func_0020C2C0(ResolutionObject *);
extern int func_0020C32C(ResolutionObject *);
extern int func_0020C2F4(ResolutionObject *);
extern int func_0020C310(ResolutionObject *);
extern int func_0020C364(int);
extern int func_0020C1B4(ResolutionObject *);
extern ResolutionObject *func_0020C478(int);
extern int func_0020D434(int);
extern int func_0020D4C8(ResolutionObject *, int);
extern int func_0020D72C(ResolutionObject *);
extern ResolutionObject *func_0020D770(int, int);
extern int func_0020D814(ResolutionObject *);
extern int func_0020F8DC(void);
extern int func_0020BFE4(void);
extern u8 func_002014C4(void);
extern u8 func_00201798(int, int, int, int);
extern int func_00044130(int, int, int);
extern void func_001F0E64(void *, int);
extern int func_001F197C(ResolutionObject *, void *, int *);
extern void func_0021D200(int, int, ResolutionObject *);
extern void func_0021D230(int, int, ResolutionObject *, int);
extern void func_0021D2C0(int, int, ResolutionObject *, int, u32, u32, u32);
extern void func_0021D334(int, int, ResolutionObject *, int, int, int, int, int, int);
extern void func_0022A7B8(ResolutionObject *, int, int, int, int, int);
extern void func_0022B06C(ResolutionObject *, u8 *);
extern void func_0022EF50(ResolutionObject *);
extern int func_0022F2BC(ResolutionObject *, int *);
extern int func_00233210(ResolutionObject *, ResolutionObject *);
extern int func_00233568(ResolutionObject *);
extern int func_00233F94(ResolutionObject *);
extern int func_002340EC(ResolutionObject *);
extern void func_0023422C(ResolutionObject *, int);
extern int func_002363FC(ResolutionObject *);
extern int func_002365BC(ResolutionObject *, ResolutionObject *, int, int);
extern int func_002379AC(int, ResolutionObject *);
extern int func_00237AF8(ResolutionObject *, int);
extern void func_00237D5C(int, int *, int *);
extern void func_00237E2C(ResolutionDescriptor *, int, int);
extern int func_0023887C(int, int, int, int);
extern void func_00239338(ResolutionDescriptor *, ResolutionObject *, ResolutionObject *, ResolutionObject *);
extern u8 func_002397D4(ResolutionRecord *, u32, u8 *);
extern int func_0025FD00(int);

int func_0022F580(ResolutionObject **input, int *outTime)
{
    ResolutionScratch local;
    ResolutionObject *excluded;
    ResolutionObject *earlyObject;
    ResolutionObject *object;
    ResolutionObject *resolvedObject;
    ResolutionDescriptor *repeatedDescriptor;
    const InitialRecord *initial;
    ResolutionRecord *recordPointer;
    int rawResultWord;
    int objectTime;
    int contextZero;
    int contextSelected;
    u32 i;
    int compactCount;
    int deletionIndex;
    int repeatedIndex;
    int transferIndex;
    int outputCount;
    int k;
    int copyWord;
    int idOffset;
    int producer;
    u32 survivors;
    int side;
    u32 selectedCount;
    u8 rawResult;
    int duration;
    int maximum;
    int time;
    int identifier;
    int quotientWork;
    int descriptorId;
    int initialThreshold;
    int threshold;
    int quarterWorking;
    int value;
    int halfAddress;
    int primary;
    int secondary;
    int mode;
    int repeated;
    StateBlock *source;
    StateBlock *sourceEnd;
    StateBlock *destination;
    u8 narrowed;
    u8 rawInput;
    u8 suppress;
    int special;
    int action;
    int actionResult;
    u32 first;
    u32 second;
    u32 third;
    u32 mix;
    u32 mainMix;
    u32 mainFirst;
    u32 mainSecond;
    u32 rerollFirst;
    u32 rerollSecond;
    u32 rerollThird;
    u32 rerollMix;
    u32 halfFirst;
    u32 halfSecond;
    u32 halfMix;

    local.selectedList = D_801F6C74;
    func_00023780(local.tail.words, 0x18);
    excluded = 0;
    suppress = 0;
    maximum = 0;
    if (func_0020C014(input[0]) != 0)
        initial = &D_801971F0[D_801976DC];
    else
        initial = &D_801971F0[D_801976E8];
    identifier = func_00022e60();
    /* The real quotient and later guard share one work word. This keeps
     * KMC's guard promotion in the retail register without an extra move. */
    quotientWork = identifier / 100;
    identifier -= quotientWork * 100;
    if (initial->bytes[12] < identifier)
        return 0;

    survivors = 0;
    selectedCount = 0;
    for (i = 0; i < 3; ++i) {
        earlyObject = input[i];
        if (func_0020C2C0(earlyObject) != 0 || func_0020C32C(earlyObject) != 0 ||
            func_0020C2F4(earlyObject) != 0 || func_0020C310(earlyObject) != 0)
            continue;
        if (earlyObject->field6C != D_801CE8C0->field00 || earlyObject->field6C >= earlyObject->field70)
            continue;
        func_0022EF50(earlyObject);
        ++survivors;
        if (earlyObject->field78 != 3) {
            excluded = earlyObject;
        } else {
            local.raw[selectedCount] = D_8018AA80[earlyObject->field7C].bytes[1];
            if (local.raw[selectedCount] >= 7) {
                excluded = earlyObject;
            } else {
                if (func_0020C364(earlyObject->field80) != 0)
                    earlyObject->field68 = func_002340EC(earlyObject);
                else
                    earlyObject->field68 = func_00233F94(earlyObject);
                local.selectedList.values[selectedCount++] = earlyObject;
            }
        }
    }
    if (selectedCount < 2)
        return 0;
    if (func_0020D814(local.selectedList.values[0]) == 1)
        return 0;
    if (selectedCount == 2) {
        if (local.selectedList.values[0]->field68 != local.selectedList.values[1]->field68)
            return 0;
    } else {
        if (local.selectedList.values[0]->field68 == local.selectedList.values[1]->field68) {
            if (local.selectedList.values[0]->field68 != local.selectedList.values[2]->field68) {
                excluded = local.selectedList.values[2];
                selectedCount = 2;
            }
        } else if (local.selectedList.values[0]->field68 == local.selectedList.values[2]->field68) {
            excluded = local.selectedList.values[1];
            selectedCount = 2;
            local.selectedList.values[1] = local.selectedList.values[2];
        } else if (local.selectedList.values[1]->field68 == local.selectedList.values[2]->field68 &&
                   local.selectedList.values[0]->field68 != local.selectedList.values[2]->field68) {
            excluded = local.selectedList.values[0];
            selectedCount = 2;
            local.selectedList.values[0] = local.selectedList.values[1];
            local.selectedList.values[1] = local.selectedList.values[2];
        }
    }
    for (i = 0; i < selectedCount; ++i) {
        /* This body-local base preserves separate record/list induction. */
        ResolutionRecord *recordBase = local.records;
        recordBase[i].bytes[0] = local.selectedList.values[i]->field80;
    }
    recordPointer = local.records;
    rawResult = func_002397D4(recordPointer, selectedCount, &local.tail.fields.mode);
    quotientWork = rawResult;
    if (quotientWork == 0xFF)
        return 0;
    rawInput = func_002014C4();
    if (selectedCount == 2 && survivors == 3) {
        func_0022F2BC(excluded, outTime);
        if (func_0020F8DC() != 0)
            return 1;
    } else if (selectedCount == 3 && survivors == 3) {
        local.tail.fields.temporaryList = D_801F6C74;
        compactCount = 0;
        for (i = 0; i < 3; ++i) {
            if ((recordPointer[i].bytes[3] & 1) != 0)
                local.tail.fields.temporaryList.values[compactCount++] = local.selectedList.values[i];
            else {
                func_0022F2BC(local.selectedList.values[i], outTime);
                if (func_0020F8DC() != 0)
                    return 1;
            }
        }
        selectedCount = compactCount;
        func_00023460(local.tail.fields.temporaryList.values, local.selectedList.values, 12);
    }
    for (i = 0; i < selectedCount; ++i) {
        local.raw[i] = D_8018AA80[local.selectedList.values[i]->field7C].bytes[1];
        if (local.raw[i] == 8)
            local.raw[i] = func_00233568(local.selectedList.values[i]);
    }
    func_00023780(&local.descriptor, 0x80);
    rawResultWord = rawResult;
    local.descriptor.header[0] = D_8018AA80[rawInput].bytes[2];
    local.descriptor.header[1] = D_8018AA80[rawInput].bytes[3];
    local.descriptor.header[2] = D_8018AA80[rawInput].bytes[4];
    local.descriptor.header[3] = D_8018AA80[rawInput].bytes[5];
    local.descriptor.header[4] = D_8018AA80[rawInput].bytes[6];
    local.descriptor.header[5] = D_8018AA80[rawInput].bytes[7];
    local.descriptor.header[6] = D_8018AA80[rawInput].bytes[8];
    local.descriptor.header[7] = D_8018AA80[rawInput].bytes[9];
    if (func_0020C364(rawResultWord) != 0) {
        producer = func_002340EC(local.selectedList.values[0]);
        side = func_0020C014(local.selectedList.values[0]) == 0;
        producer = func_0023887C(producer, local.tail.fields.mode, side, rawResultWord);
    } else {
        producer = func_00233F94(local.selectedList.values[0]);
        side = func_0020C014(local.selectedList.values[0]);
        producer = func_0023887C(producer, local.tail.fields.mode, side, rawResultWord);
    }
    local.descriptor.header[8] = local.tail.fields.mode;
    func_00237E2C(&local.descriptor, local.tail.fields.mode, producer);
    if (local.descriptor.count == 0) {
        for (i = 0; i < selectedCount; ++i) {
            D_801CE8C0->field818++;
            earlyObject = local.selectedList.values[i];
            {
                int incomingTime;
                int zeroWork;
                zeroWork = earlyObject->field94;
                incomingTime = *outTime;
                if (zeroWork < incomingTime)
                    zeroWork = incomingTime;
                earlyObject->field94 = zeroWork;
                contextZero = D_801CE8C0->field828;
                {
                    int publication = contextZero;
                    if ((u32)contextZero < (u32)zeroWork)
                        publication = zeroWork;
                    earlyObject->field94 = publication;
                    func_0021D200(0x1C, publication, earlyObject);
                }
            }
            earlyObject->field6C++;
            contextZero = earlyObject->field94;
            objectTime = contextZero;
            *outTime = objectTime;
        }
        return 0;
    }
    func_00239338(&local.descriptor, local.selectedList.values[0], local.selectedList.values[1], local.selectedList.values[2]);
    narrowed = func_0025FD00(rawResult);
    source = (StateBlock *)(local.selectedList.values[0]->nested + 0x44);
    destination = local.copiedState;
    sourceEnd = (StateBlock *)(local.selectedList.values[0]->nested + 0x94);
    do {
        *destination++ = *source++;
    } while (source != sourceEnd);
    func_001F0E64(local.copiedState,
        func_00201798(local.selectedList.values[0]->field48, local.selectedList.values[0]->field4C,
                      local.selectedList.values[0]->field7C, 0) & 0xFF);
    duration = func_001F197C(local.selectedList.values[0], local.copiedState, local.tail.words);
    if (func_0020BFE4() == 0) {
        duration = 10;
        local.tail.words[0] = 5;
    }
    time = *outTime - local.tail.words[0];
    for (i = 0; i < selectedCount; ++i)
        if (time < local.selectedList.values[i]->field94)
            time = local.selectedList.values[i]->field94;
    for (i = 0; i < local.descriptor.count; ++i) {
        descriptorId = local.descriptor.ids[i];
        if (descriptorId < 20)
            resolvedObject = func_0020C478(descriptorId);
        else {
            func_00237D5C(descriptorId, &local.tail.fields.out0, &local.tail.fields.out1);
            resolvedObject = func_0020D770(local.tail.fields.out0, local.tail.fields.out1);
        }
        if (time < resolvedObject->field94)
            time = resolvedObject->field94;
    }
    D_801CE8C0->field818++;
    for (i = 0; i < selectedCount; ++i) {
        action = (u8)func_00044130(((u8 *)&local.selectedList.values[i]->field48)[3],
                              ((u8 *)&local.selectedList.values[i]->field4C)[3],
                              func_0020D434(local.selectedList.values[i]->field58) & 0xFF);
        actionResult = func_00201798(local.selectedList.values[i]->field48,
                                    local.selectedList.values[i]->field4C, action, 0);
        earlyObject = local.selectedList.values[i];
        objectTime = earlyObject->field94;
        if (objectTime < time)
            objectTime = time;
        earlyObject->field94 = objectTime;
        contextSelected = D_801CE8C0->field828;
        {
            int publication = contextSelected;
            if ((u32)contextSelected < (u32)objectTime)
                publication = objectTime;
            earlyObject->field94 = publication;
            func_0021D200(0x1C, publication, earlyObject);
        }
        earlyObject->field6C++;
        contextSelected = earlyObject->field94;
        time = contextSelected;
        local.tail.fields.selectedTimes[i] = time;
        if (func_0020BFE4() != 0)
            func_0021D2C0(0xB, time, local.selectedList.values[i], 0xFF, action, rawResult, actionResult);
        else
            func_0021D2C0(0xB, time, local.selectedList.values[i], 0, action, rawResult, 0);
        func_0021D230(0x3E, time, local.selectedList.values[i], local.raw[i]);
    }
    D_801CE8C0->field823++;
    ++time;
    if (local.descriptor.header[8] != 8 && local.descriptor.header[8] != 9 &&
        local.descriptor.header[8] != 10) {
        if (producer >= 47)
            suppress = 1;
        else if (producer >= 20) {
            func_00237D5C(producer, &local.tail.fields.out2, &local.tail.fields.out3);
            earlyObject = func_0020D770(local.tail.fields.out2, local.tail.fields.out3);
            if (func_0020C2C0(earlyObject) != 0)
                suppress = 1;
        }
    }
    for (i = 0; i < local.descriptor.count; ++i) {
        special = 0;
        idOffset = i * 4;
        /* The actual ID address, then the later secondary result, share a word. */
        secondary = idOffset + (int)&local;
        object = func_0020C478(((int *)secondary)[14]);
        if (object == 0)
            continue;
        if (func_0020C2C0(object) != 0) {
            deletionIndex = i;
            if (deletionIndex < local.descriptor.count - 1) {
                copyWord = secondary;
                do {
                    ((int *)copyWord)[14] = local.descriptor.ids[deletionIndex + 1];
                    local.descriptor.halfwords[deletionIndex] = local.descriptor.halfwords[deletionIndex + 1];
                    local.descriptor.bytes[deletionIndex] = local.descriptor.bytes[deletionIndex + 1];
                    local.descriptor.values[deletionIndex] = local.descriptor.values[deletionIndex + 1];
                    ++deletionIndex;
                    copyWord += 4;
                } while (deletionIndex < local.descriptor.count - 1);
            }
            --i;
            --local.descriptor.count;
            continue;
        }
        initialThreshold = func_002363FC(object);
        /* The real halfword address stays live through the three random calls. */
        halfAddress = i * 2;
        halfAddress += (int)&local;
        threshold = func_002365BC(local.selectedList.values[0], object,
                                  ((u16 *)halfAddress)[48], initialThreshold);
        mainFirst = rand();
        mainSecond = rand();
        third = rand();
        mainMix = ((mainFirst << 18) & 0x0C000000U) | (mainSecond << 15) | third;
        value = ((s16 *)halfAddress)[68];
        primary = (u32)threshold > (mainMix % 100U);
        if (local.descriptor.header[3] == 9) {
            if (func_0020C1B4(object) != 0) {
                value = 0;
                primary = 0;
            }
        } else if (local.descriptor.header[3] == 8) {
            rerollFirst = rand();
            rerollSecond = rand();
            rerollThird = rand();
            primary = threshold * 3 / 4;
            rerollMix = ((rerollFirst << 18) & 0x0C000000U) | (rerollSecond << 15) | rerollThird;
            primary = (u32)primary > (rerollMix % 100U);
        }
        if (primary != 0)
            primary = func_002379AC(local.descriptor.header[1], object);
        local.descriptor.primary[i] = primary;
        secondary = 0;
        local.tail.fields.modifier = 0;
        if (local.descriptor.header[4] == 8) {
            local.tail.fields.modifier = local.descriptor.header[5];
            switch (local.descriptor.header[7]) {
            case 4:
                if (primary != 0) {
                    halfFirst = rand();
                    halfSecond = rand();
                    third = rand();
                    secondary = threshold / 2;
                    halfMix = ((halfFirst << 18) & 0x0C000000U) | (halfSecond << 15) | third;
                    secondary = (u32)secondary > (halfMix % 100U);
                }
                break;
            case 5:
                secondary = primary;
                break;
            case 6:
                if (primary != 0 && value >= object->field20)
                    secondary = 1;
                break;
            case 2:
                secondary = 1;
                break;
            case 11:
                if (primary != 0) {
                    quarterWorking = threshold;
                    first = rand();
                    second = rand();
                    third = rand();
                    secondary = quarterWorking / 4;
                    mix = ((first << 18) & 0x0C000000U) | (second << 15) | third;
                    secondary = (u32)secondary > (mix % 100U);
                }
                break;
            default:
                break;
            }
            if (secondary != 0)
                secondary = func_002379AC(local.tail.fields.modifier, object);
        }
        local.descriptor.secondary[i] = secondary;
        if (local.descriptor.header[1] == 1) {
            if (primary != 0) {
                func_0022A7B8(object, 0, time + local.tail.words[0], value, narrowed, 0);
                mode = -1;
                if (D_8018AA80[rawInput].bytes[1] == 10)
                    mode = 1;
                else if (D_8018AA80[rawInput].bytes[1] == 2)
                    mode = 2;
                if (mode != -1) {
                    repeatedDescriptor = &local.descriptor;
                    /* The real counter initialization follows the index here
                     * to preserve KMC scheduling before delay-slot filling. */
                    for (repeatedIndex = i, repeated = 0; repeatedIndex < local.descriptor.count; ++repeatedIndex)
                        repeated += repeatedDescriptor->ids[i] == repeatedDescriptor->ids[repeatedIndex];
                    if ((repeated ^ 1) == 0)
                        special = func_00237AF8(object, mode);
                }
                if (special != 0 && func_0020C2C0(object) == 0 && func_0020C32C(object) == 0) {
                    func_0021D334(0x1F, time + local.tail.words[0], object, 13,
                        value >> 8, value & 0xFF, narrowed, D_801CE8C0->field823, (u8)special);
                    func_0023422C(object, special);
                } else {
                    if (secondary != 0)
                        func_0022B06C(object, &local.tail.fields.modifier);
                    else
                        local.tail.fields.modifier = 0;
                    func_0021D334(0x1F, time + local.tail.words[0], object, 0,
                        value >> 8, value & 0xFF, narrowed, D_801CE8C0->field823,
                        local.tail.fields.modifier);
                    object->field94 = time + local.tail.words[0];
                }
            } else {
                value /= 2;
                if (value <= 0)
                    value = 1;
                if (value >= 10000)
                    value = 9999;
                func_0021D334(0x1F, time + local.tail.words[0], object, 1,
                    value >> 8, value & 0xFF, narrowed, D_801CE8C0->field823, 0);
                func_0022A7B8(object, 0, time + local.tail.words[0], value, narrowed, 0);
                object->field94 = time + local.tail.words[0];
            }
            if (func_0020C2C0(object) != 0 && func_0020C014(local.selectedList.values[0]) != 0) {
                for (transferIndex = 0; transferIndex < selectedCount; ++transferIndex) {
                    k = func_00233210(local.selectedList.values[transferIndex], object);
                    if (k != 0) {
                        local.selectedList.values[transferIndex]->field34 += k;
                        func_0021D230(0x3F, local.tail.fields.selectedTimes[transferIndex], local.selectedList.values[transferIndex], k);
                    }
                }
            }
        } else if (local.descriptor.header[1] == 2) {
            k = func_0020D4C8(object, value);
            object->field20 += k;
            if (secondary != 0)
                func_0022B06C(object, &local.tail.fields.modifier);
            else
                local.tail.fields.modifier = 0;
            func_0021D334(0x1F, time + local.tail.words[0], object, 2,
                value >> 8, value & 0xFF, narrowed, D_801CE8C0->field823, local.tail.fields.modifier);
            object->field94 = time + local.tail.words[0];
        } else if (local.descriptor.header[1] == 8) {
            if (primary != 0) {
                object->field40 |= 8;
                if (secondary != 0)
                    func_0022B06C(object, &local.tail.fields.modifier);
                else
                    local.tail.fields.modifier = 0;
                func_0021D334(0x1F, time + local.tail.words[0], object, 3,
                    0, 0, narrowed, D_801CE8C0->field823, local.tail.fields.modifier);
            } else {
                func_0021D334(0x1F, time + local.tail.words[0], object, 6,
                    0, 0, narrowed, D_801CE8C0->field823, 0);
            }
            object->field94 = time + local.tail.words[0];
        } else if (local.descriptor.header[1] == 10) {
            if (primary != 0) {
                object->field40 |= 16;
                if (secondary != 0)
                    func_0022B06C(object, &local.tail.fields.modifier);
                else
                    local.tail.fields.modifier = 0;
                func_0021D334(0x1F, time + local.tail.words[0], object, 7,
                    0, 0, narrowed, D_801CE8C0->field823, local.tail.fields.modifier);
            } else {
                func_0021D334(0x1F, time + local.tail.words[0], object, 6,
                    0, 0, narrowed, D_801CE8C0->field823, 0);
            }
            object->field94 = time + local.tail.words[0];
        } else if (local.descriptor.header[1] == 4) {
            if (primary != 0) {
                object->field40 |= 4;
                if (secondary != 0)
                    func_0022B06C(object, &local.tail.fields.modifier);
                else
                    local.tail.fields.modifier = 0;
                func_0021D334(0x1F, time + local.tail.words[0], object, 4,
                    0, 0, narrowed, D_801CE8C0->field823, local.tail.fields.modifier);
            } else {
                func_0021D334(0x1F, time + local.tail.words[0], object, 6,
                    0, 0, narrowed, D_801CE8C0->field823, local.tail.fields.modifier);
            }
            object->field94 = time + local.tail.words[0];
        }
        if (maximum < object->field94)
            maximum = object->field94;
    }
    --time;
    outputCount = 0;
    for (i = 0; i < selectedCount; ++i)
        local.tail.fields.selectedIds[i] = func_0020D72C(local.selectedList.values[i]);
    local.tail.fields.selectedIds[i] = -1;
    if (local.descriptor.header[8] != 8 && local.descriptor.header[8] != 9 &&
        local.descriptor.header[8] != 10) {
        if (suppress != 0) {
            local.tail.fields.ids[outputCount] = producer;
            local.tail.fields.flags[outputCount++] = 0;
        }
    }
    for (i = 0; i < local.descriptor.count; ++i) {
        local.tail.fields.ids[outputCount] = local.descriptor.ids[i];
        local.tail.fields.flags[outputCount++] = local.descriptor.secondary[i];
    }
    local.tail.fields.flags[outputCount] = -1;
    local.tail.fields.ids[outputCount] = -1;
    func_0021D2C0(0x16, time + local.tail.words[0], 0, rawResult,
        (u32)local.tail.fields.selectedIds, (u32)local.tail.fields.ids, (u32)local.tail.fields.flags);
    if (maximum < time + local.tail.words[0])
        maximum = time + local.tail.words[0];
    for (i = 0; i < selectedCount; ++i) {
        local.selectedList.values[i]->field84 = local.tail.words[0];
        if (duration >= maximum - time - local.tail.words[0])
            local.selectedList.values[i]->field88 = duration;
        else
            local.selectedList.values[i]->field88 = maximum - time - local.tail.words[0];
        local.selectedList.values[i]->field8C = 0;
        local.selectedList.values[i]->field94 = time + local.tail.words[0] + local.selectedList.values[i]->field88;
        if ((u32)*outTime < (u32)local.selectedList.values[i]->field94)
            *outTime = local.selectedList.values[i]->field94;
    }
    *outTime += 0x200;
    D_801CE8C0->field828 = *outTime;
    func_0021D230(0x20, *outTime, 0, D_801CE8C0->field823);
    return 1;
}
