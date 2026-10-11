typedef unsigned char u8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;

typedef struct DispatchObject {
    u8 unknown00[0x20];
    u16 field20;
    u8 unknown22[0x26];
    int field48;
    int field4C;
    u8 unknown50[4];
    int field54;
    int field58;
    u8 unknown5C[0x1C];
    u8 field78;
    u8 unknown79[3];
    u32 field7C;
    u8 unknown80[0x14];
    int field94;
    u8 unknown98[0x60];
} DispatchObject;

typedef struct DispatchContext {
    u8 unknown00[0x818];
    u8 field818;
    u8 unknown819[0xA];
    u8 field823;
    u8 unknown824[4];
    int field828;
} DispatchContext;

typedef struct DispatchDescriptor {
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
} DispatchDescriptor;

/* Observed byte/list offsets: A8, B0, B8 and E8; gap bytes are unconsumed. */
typedef struct DispatchLists {
    u8 modifier;
    u8 unknown01[7];
    int single[2];
    int ids[12];
    int flags[12];
} DispatchLists;

extern u8 *D_801CE8BC;
extern DispatchContext *D_801CE8C0;
typedef struct InputRecord { u8 bytes[16]; } InputRecord;
extern const InputRecord D_8018AA80[];
extern void func_00023780(void *, u32);
extern u32 rand(void);
extern void func_0020D870(DispatchObject *, int);
extern int func_00233F94(DispatchObject *);
extern int func_0020C014(DispatchObject *);
extern int func_0023887C(int, int, int, int);
extern void func_00237E2C(DispatchDescriptor *, int, int);
extern void func_00239338(DispatchDescriptor *, DispatchObject *, DispatchObject *, DispatchObject *);
extern int func_0025FD00(int);
extern void func_0021D1CC(int, int);
extern DispatchObject *func_0020C478(int);
extern int func_0020C2C0(DispatchObject *);
extern int func_0020C0CC(DispatchObject *);
extern int func_0020BFF8(DispatchObject *);
extern int func_0020C32C(DispatchObject *);
extern int func_002363FC(DispatchObject *);
extern int func_002365BC(DispatchObject *, DispatchObject *, int, int);
extern int func_002379AC(int, DispatchObject *);
extern void func_0022A7B8(DispatchObject *, int, int, int, int, int);
extern int func_00237AF8(DispatchObject *, int);
extern void func_0021D334(int, int, DispatchObject *, int, int, int, int, int, int);
extern void func_0023422C(DispatchObject *, int);
extern void func_0022B06C(DispatchObject *, u8 *);
extern int func_0020D72C(DispatchObject *);
extern void func_0021D2C0(int, int, int, int, int *, int *, int *);
extern void func_0021D230(int, int, DispatchObject *, int);

int func_00232458(u8 *input, int *outTime)
{
    DispatchDescriptor descriptor;
    DispatchLists lists;
    DispatchObject *temporary;
    DispatchObject *object;
    DispatchObject *scanObject;
    u8 *global;
    int i;
    int j;
    int copyWord;
    int offset;
    int idOffset;
    int identifierOrNumerator;
    int initialInput2;
    int side;
    int producer;
    int narrowed;
    int time;
    int maximum;
    int threshold;
    int value;
    u8 *valueAddress;
    int primary;
    int secondary;
    int mode;
    int repeated;
    int producerInputOrRepeatLimit;
    int repeatPredicate;
    int special;
    /* The early identifier and later signed quarter numerator have disjoint uses.
       Keeping their real phases in one word preserves the call-crossing constraint. */
    u32 third;
    u32 mainMix;
    u32 mainFirst;
    u32 mainSecond;

    func_00023780(&descriptor, 0x80);
    descriptor.header[0] = D_8018AA80[input[2]].bytes[2];
    descriptor.header[1] = D_8018AA80[input[2]].bytes[3];
    descriptor.header[2] = D_8018AA80[input[2]].bytes[4];
    descriptor.header[3] = D_8018AA80[input[2]].bytes[5];
    descriptor.header[4] = D_8018AA80[input[2]].bytes[6];
    descriptor.header[5] = D_8018AA80[input[2]].bytes[7];
    descriptor.header[6] = D_8018AA80[input[2]].bytes[8];
    descriptor.header[7] = D_8018AA80[input[2]].bytes[9];
    descriptor.header[8] = 10;
    i = 0;
    global = D_801CE8BC;
    offset = 0x1C4;
    for (; i < 20; ++i, offset += 0xF8) {
        temporary = (DispatchObject *)(global + offset);
        if (temporary->field48 == 0 && temporary->field4C == 0)
            break;
    }
    func_0020D870(temporary, 1);
    temporary->field58 = 7;
    temporary->field54 = 1;
    initialInput2 = input[2];
    temporary->field78 = 3;
    temporary->field7C = initialInput2;
    identifierOrNumerator = func_00233F94(temporary);
    side = func_0020C014(temporary);
    /* The real fourth producer argument and later scan limit have disjoint uses. */
    producerInputOrRepeatLimit = input[1];
    producer = func_0023887C(identifierOrNumerator, descriptor.header[8], side, producerInputOrRepeatLimit);
    /* Hard mode10 selects six producer iterations; later deletion only shrinks it. */
    func_00237E2C(&descriptor, descriptor.header[8], producer);
    if (descriptor.count == 0) {
        func_00023780(temporary, 0xF8);
        return 0;
    }
    func_00239338(&descriptor, temporary, temporary, temporary);
    narrowed = (s16)func_0025FD00(input[1]);
    time = 1;
    D_801CE8C0->field818++;
    func_0021D1CC(0x40, 0);
    D_801CE8C0->field823++;
    maximum = 0;
    for (i = 0; i < descriptor.count; ++i) {
        special = 0;
        idOffset = i * 4;
        /* Actual ID address precedes the random/result phase of this MIPS word. */
        secondary = idOffset + (int)&descriptor;
        object = func_0020C478(((int *)secondary)[4]);
        if (object == 0)
            continue;
        if (func_0020C2C0(object) != 0) {
            j = i;
            if (j < descriptor.count - 1) {
                do {
                    descriptor.ids[j] = descriptor.ids[j + 1];
                    descriptor.halfwords[j] = descriptor.halfwords[j + 1];
                    descriptor.bytes[j] = descriptor.bytes[j + 1];
                    descriptor.values[j] = descriptor.values[j + 1];
                    ++j;
                } while (j < descriptor.count - 1);
            }
            --i;
            --descriptor.count;
            continue;
        }
        threshold = func_002363FC(object);
        valueAddress = (u8 *)(i * 2 + (int)&descriptor);
        threshold = func_002365BC(temporary, object, ((u16 *)valueAddress)[28], threshold);
        mainFirst = rand();
        mainSecond = rand();
        third = rand();
        mainMix = ((mainFirst << 18) & 0x0C000000U) | (mainSecond << 15) | third;
        value = ((s16 *)valueAddress)[48];
        primary = (u32)threshold > mainMix % 100U;
        descriptor.primary[i] = primary;
        secondary = 0;
        lists.modifier = 0;
        if (descriptor.header[4] == 8) {
            lists.modifier = descriptor.header[5];
            switch (descriptor.header[7]) {
            case 4:
                if (primary != 0) {
                    u32 halfFirst;
                    u32 halfSecond;
                    u32 halfMix;
                    halfFirst = rand();
                    halfSecond = rand();
                    third = rand();
                    halfMix = ((halfFirst << 18) & 0x0C000000U) | (halfSecond << 15) | third;
                    secondary = threshold / 2;
                    secondary = (u32)secondary > halfMix % 100U;
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
                    u32 quarterSecond;
                    u32 quarterMix;
                    u32 quarterThird;
                    secondary = rand();
                    quarterSecond = rand();
                    quarterThird = rand();
                    identifierOrNumerator = threshold;
                    if (identifierOrNumerator < 0)
                        identifierOrNumerator += 3;
                    quarterMix = (((u32)secondary << 18) & 0x0C000000U) | (quarterSecond << 15) | quarterThird;
                    secondary = identifierOrNumerator >> 2;
                    secondary = (u32)secondary > quarterMix % 100U;
                }
                break;
            default:
                break;
            }
            if (secondary != 0)
                secondary = func_002379AC(lists.modifier, object);
        }
        descriptor.secondary[i] = secondary;
        if (primary != 0) {
            func_0022A7B8(object, 0, time, value, narrowed, 0);
            mode = -1;
            if (D_8018AA80[input[2]].bytes[1] == 10)
                mode = 1;
            else if (D_8018AA80[input[2]].bytes[1] == 2)
                mode = 2;
            if (mode != -1) {
                producerInputOrRepeatLimit = descriptor.count;
                {
                    DispatchDescriptor *repeatDescriptor = &descriptor;
                    int repeatCursor;
                    int repeatCount;
                    repeatCursor = i;
                    repeatCount = 0;
                    for (; repeatCursor < producerInputOrRepeatLimit; ++repeatCursor)
                        repeatCount += repeatDescriptor->ids[i] == repeatDescriptor->ids[repeatCursor];
                    repeated = repeatCount;
                }
                repeatPredicate = repeated ^ 1;
                if (repeatPredicate == 0)
                    special = func_00237AF8(object, mode);
            }
            if (special != 0 && func_0020C2C0(object) == 0 && func_0020C32C(object) == 0) {
                func_0021D334(0x1F, time, object, 13, value >> 8, value & 0xFF,
                             narrowed, D_801CE8C0->field823, (u8)special);
                func_0023422C(object, special);
            } else {
                if (secondary != 0)
                    func_0022B06C(object, &lists.modifier);
                func_0021D334(0x1F, time, object, 0, value >> 8, value & 0xFF,
                             narrowed, D_801CE8C0->field823, lists.modifier);
                object->field94 = time;
            }
        } else {
            value /= 2;
            if (value <= 0)
                value = 1;
            if (value >= 10000)
                value = 9999;
            func_0021D334(0x1F, time, object, 1, value >> 8, value & 0xFF,
                         narrowed, D_801CE8C0->field823, 0);
            func_0022A7B8(object, 0, time, value, narrowed, 0);
            object->field94 = time;
        }
        if (maximum < object->field94)
            maximum = object->field94;
    }
    for (i = 0; i < 20; ++i) {
        scanObject = func_0020C478(i);
        if (func_0020C2C0(scanObject) != 0)
            continue;
        if (func_0020C0CC(scanObject) != 0)
            continue;
        if (func_0020BFF8(scanObject) == 0 && temporary != scanObject)
            break;
    }
    lists.single[0] = func_0020D72C(scanObject);
    lists.single[1] = -1;
    --time;
    for (i = 0; i < descriptor.count; ++i) {
        lists.ids[i] = descriptor.ids[i];
        lists.flags[i] = 0;
    }
    lists.flags[i] = -1;
    lists.ids[i] = -1;
    func_0021D2C0(0x16, time, 0, input[1], lists.single, lists.ids, lists.flags);
    func_0021D230(0x3E, time, scanObject, 8);
    /* This final maximum update is otherwise dead. KMC late jump cleanup
       leaves its spill reload in the retail code; it does not set outTime. */
    if (maximum < time)
        maximum = time;
    *outTime += 0x200;
    D_801CE8C0->field828 = *outTime;
    func_0021D230(0x20, *outTime, 0, D_801CE8C0->field823);
    func_00023780(temporary, 0xF8);
    return 1;
}
