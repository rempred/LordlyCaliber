typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef struct A9CObject {
    u8 *pointers[3];
    u8 unknown0C[0x3C];
    int field48;
    int field4C;
    u8 unknown50[7];
    u8 field57;
    u8 unknown58[3];
    u8 field5B;
    int field5C;
    u8 unknown60[4];
    int field64;
    int field68;
    int field6C;
    int field70;
    u8 unknown74[8];
    u32 field7C;
    u8 unknown80[4];
    int field84;
    int field88;
    int field8C;
    u8 unknown90[4];
    int field94;
} A9CObject;

typedef struct A9CContext {
    int field00;
    u8 unknown04[0x814];
    u8 field818;
    u8 unknown819[0xF];
    int field828;
} A9CContext;

typedef struct A9CStateBlock { int words[4]; } A9CStateBlock;

/* Preserve the observed output/state adjacency, including evaluator writes
 * into the copied state. The 8-byte clear does not prove an output bound;
 * behavior beyond these 88 observed bytes remains a data-domain question. */
typedef union A9CWork {
    int words[22];
    struct {
        int output[2];
        A9CStateBlock state[5];
    } fields;
} A9CWork;

extern A9CContext *D_801CE8C0;
/* The ten accessed words span both existing original data owners. */
extern const float D_801F68D0[];
extern void func_00023780(void *, u32);
extern int func_00022e60(void);
extern u32 rand(void);
extern int func_000454E0(u16);
extern int func_0020C104(A9CObject *);
extern int func_0020C2C0(A9CObject *);
extern int func_0020C32C(A9CObject *);
extern int func_0020C2F4(A9CObject *);
extern int func_0020C310(A9CObject *);
extern int func_00233ED4(A9CObject *);
extern A9CObject *func_0020C478(int);
extern int func_0022F2BC(A9CObject *, int *);
extern int func_0020F8DC(void);
extern int func_0020D814(A9CObject *);
extern void func_0022EF50(A9CObject *);
extern int func_0020BFE4(void);
extern int func_0020BFF8(A9CObject *);
extern u16 func_0020C448(A9CObject *);
extern u8 func_00201584(int, int);
extern u8 func_00201798(int, int, int, int);
extern void func_001F0E04(void *, int);
extern void func_001F0E64(void *, int);
extern int func_001F197C(A9CObject *, void *, int *);
extern int func_0022222C(A9CObject *, A9CObject *, int, int);
extern int func_002223E0(A9CObject *, A9CObject *, int, int, int, int);
extern void func_0021D200(int, int, A9CObject *);
extern void func_0021D230(int, int, A9CObject *, int);
extern void func_0021D25C(int, int, A9CObject *, int, int);
extern void func_0021D28C(int, int, A9CObject *, int, int, int);
extern void func_0021D2C0(int, int, A9CObject *, int, int, int, int);
extern int func_0020D72C(A9CObject *);
extern int func_002361EC(A9CObject *);
extern int func_002363FC(A9CObject *);
extern int func_002365BC(A9CObject *, A9CObject *, int, int);
extern int func_00235754(A9CObject *, A9CObject *);
extern int func_0023697C(A9CObject *, A9CObject *, float, int, int, int);
extern int func_0022A4E0(A9CObject *);
extern void func_0022ADFC(A9CObject *, int, int, int, int, int, int, int);

int func_00230A9C(A9CObject **input, int *outTime)
{
    int raw[3];
    A9CObject *selected[3];
    int scheduled[3];
    int values[3];
    A9CWork work;
    A9CObject *object;
    A9CObject *firstDestination;
    int firstResult;
    A9CObject *target;
    A9CObject *pendingTarget;
    A9CObject *excluded;
    A9CStateBlock *source;
    A9CStateBlock *sourceEnd;
    A9CStateBlock *destination;
    A9CStateBlock *failureState;
    A9CStateBlock *failureSourceEnd;
    u32 selectedCount;
    u32 i;
    u32 j;
    int base;
    int publication;
    int objectTime;
    int deficitObjectTime;
    int publicationTime;
    int narrowed;
    int firstEvaluation;
    int duration;
    /* Separate single-definition operands preserve retail call scheduling. */
    int firstOperand;
    int innerOperand;
    int innerResult;
    int threshold;
    int decision;
    int rawMode;
    int selector;
    int startTime;
    int endTime;
    int endpoint;
    int successExtra;
    int failureExtra;
    int subtype;
    int hitCount;
    int randomWord;
    int secondEvaluation;
    int followup;
    int failureSelector;
    int auxiliary;
    int helperResult;
    u32 first;
    u32 second;
    u32 third;
    u32 mixed;
    int result;
    int auxiliaryGuard;
    float rounded;
    u8 byte57;
    u8 byte5B;

    func_00023780(work.words, 8);
    auxiliaryGuard = 0;
    for (i = 0; i < 3; ++i) {
        raw[i] = -1;
        if (func_0020C104(input[i]) == 0)
            continue;
        if (func_0020C2C0(input[i]) != 0)
            continue;
        if (func_0020C32C(input[i]) != 0)
            continue;
        if (func_0020C2F4(input[i]) != 0)
            continue;
        if (func_0020C310(input[i]) != 0)
            continue;
        if (input[i]->field6C != D_801CE8C0->field00)
            continue;
        if (input[i]->field6C >= input[i]->field70)
            continue;
        firstResult = func_00233ED4(input[i]);
        firstDestination = input[i];
        raw[i] = firstResult;
        firstDestination->field68 = firstResult;
    }

    if (raw[0] != -1 && raw[0] == raw[1] && raw[0] == raw[2]) {
        for (i = 0; i < 3; ++i)
            selected[i] = input[i];
        selectedCount = 3;
        pendingTarget = func_0020C478(raw[0]);
        target = pendingTarget;
    } else {
        if (raw[0] != -1 && raw[0] == raw[1]) {
            raw[2] = -1;
            selected[0] = input[0];
            selected[1] = input[1];
            pendingTarget = func_0020C478(raw[0]);
            excluded = input[2];
        } else if (raw[0] != -1 && raw[0] == raw[2]) {
            raw[1] = -1;
            selected[0] = input[0];
            selected[1] = input[2];
            pendingTarget = func_0020C478(raw[0]);
            excluded = input[1];
        } else {
            if (raw[1] == -1 || raw[1] != raw[2])
                return 0;
            raw[0] = -1;
            selected[0] = input[1];
            selected[1] = input[2];
            pendingTarget = func_0020C478(raw[1]);
            excluded = input[0];
        }
        selectedCount = 2;
        target = pendingTarget;
        if (excluded != 0) {
            func_0022F2BC(excluded, outTime);
            if (func_0020F8DC() != 0)
                return 1;
        }
    }
    if (func_0020D814(selected[0]) == 1)
        return 0;
    if (func_0020C2C0(target) != 0)
        return 0;
    byte57 = target->field57;
    byte5B = target->field5B;
    for (i = 0; i < selectedCount; ++i)
        func_0022EF50(selected[i]);

    if ((u32)target->field94 < (u32)*outTime)
        base = *outTime;
    else
        base = target->field94;
    if (func_0020BFE4() != 0) {
        for (i = 0; i < selectedCount; ++i) {
            values[i] = func_0022222C(selected[i], target,
                                     selected[i]->field5C, selected[i]->field64);
            scheduled[i] = base - values[i];
        }
    } else {
        for (i = 0; i < selectedCount; ++i) {
            values[i] = 0;
            scheduled[i] = base;
        }
    }
    base = 0;
    for (i = 0; i < selectedCount; ++i) {
        deficitObjectTime = selected[i]->field94;
        if (scheduled[i] < deficitObjectTime && base < deficitObjectTime - scheduled[i])
            base = deficitObjectTime - scheduled[i];
    }
    D_801CE8C0->field818++;
    if (selectedCount != 0) {
        i = 0;
        do {
            scheduled[i] += base;
            object = selected[i];
            publicationTime = object->field94;
            if (publicationTime < scheduled[i])
                publicationTime = scheduled[i];
            object->field94 = publicationTime;
            if ((u32)D_801CE8C0->field828 < (u32)publicationTime)
                publication = publicationTime;
            else
                publication = D_801CE8C0->field828;
            object->field94 = publication;
            func_0021D200(0x1C, publication, object);
            object->field6C++;
            scheduled[i] = object->field94;
            ++i;
        } while (i < selectedCount);
    }
    narrowed = func_000454E0((u16)func_0020C448(selected[0]));
    for (i = 0; i < selectedCount; ++i)
        func_0021D230(0x3E, scheduled[i], selected[i], (u8)narrowed);

    if (func_0020BFE4() != 0) {
        for (i = 0; i < selectedCount; ++i)
            func_0021D28C(3, scheduled[i], selected[i], 0xFF,
                         selected[i]->field68, 5);
        source = (A9CStateBlock *)(selected[0]->pointers[0] + 0x44);
        sourceEnd = (A9CStateBlock *)(selected[0]->pointers[0] + 0x94);
        destination = work.fields.state;
        do {
            *destination++ = *source++;
        } while (source != sourceEnd);
        func_001F0E64(work.fields.state, 6);
        base = func_001F197C(selected[0], work.fields.state, 0);
        for (i = 0; i < selectedCount; ++i) {
            func_0021D25C(5, scheduled[i] + values[i], selected[i], 0xFF, 6);
            values[i] += base;
        }
    }

    firstOperand = func_002361EC(selected[0]);
    threshold = func_002363FC(target);
    /* The cutoff is consumed before this word becomes the decision. */
    decision = func_002365BC(selected[0], target, firstOperand, threshold);
    first = rand();
    second = rand();
    third = rand();
    mixed = ((first << 18) & 0x0C000000U) | (second << 15) | third;
    mixed %= 100U;
    decision = mixed < (u32)decision;
    source = (A9CStateBlock *)(selected[0]->pointers[0] + 0x44);
    sourceEnd = (A9CStateBlock *)(selected[0]->pointers[0] + 0x94);
    destination = work.fields.state;
    do {
        *destination++ = *source++;
    } while (source != sourceEnd);
    rawMode = 0;
    if (decision == 0) {
        if (func_00201584(target->field48, target->field4C) != 0)
            rawMode = 2;
        else
            rawMode = 1;
    }
    selector = (u8)func_00201798(selected[0]->field48, selected[0]->field4C,
                                selected[0]->field7C, rawMode);
    func_001F0E64(work.fields.state, selector);
    duration = func_001F197C(selected[0], work.fields.state, work.words);
    if (func_0020BFE4() == 0) {
        duration = 15;
        work.words[0] = 10;
    }
    for (i = 0; i < selectedCount; ++i) {
        if (func_0020BFE4() != 0) {
            if (decision != 0)
                func_0021D28C(6, scheduled[i] + values[i], selected[i], 0xFF,
                             selector, selected[i]->field7C);
            else
                func_0021D28C(7, scheduled[i] + values[i], selected[i], 0xFF,
                             selector, selected[i]->field7C);
        } else {
            if (decision != 0)
                func_0021D28C(6, scheduled[i], selected[i], 0,
                             0, selected[i]->field7C);
            else
                func_0021D28C(7, scheduled[i], selected[i], 0,
                             0, selected[i]->field7C);
        }
    }
    startTime = scheduled[0] + values[0];
    endTime = startTime + duration;
    endpoint = startTime + work.words[0] - 5;
    if (decision != 0) {
        if (func_0020BFE4() != 0) {
            helperResult = func_0020D72C(selected[0]);
            func_0021D2C0(0x17, endpoint, 0, 0x5D, (u8)narrowed,
                         helperResult, selected[0]->field68);
        }
        if (endpoint < startTime)
            startTime = endpoint;
        successExtra = func_0020BFE4() != 0 ? 25 : 5;
        if (endTime < successExtra + endpoint)
            endTime = successExtra + endpoint;
    } else if (func_00201584(target->field48, target->field4C) != 0) {
        subtype = func_0020BFF8(target) != 0 ? 0x60 : 0x5F;
        if (func_0020BFE4() != 0) {
            helperResult = func_0020D72C(selected[0]);
            func_0021D2C0(0x17, endpoint, 0, subtype, (u8)narrowed,
                         helperResult, selected[0]->field68);
        }
        if (endpoint < startTime)
            startTime = endpoint;
        failureExtra = func_0020BFE4() != 0 ? 15 : 5;
        if (endTime < endpoint + failureExtra)
            endTime = endpoint + failureExtra;
    }

    if (decision != 0) {
        hitCount = 0;
        for (i = 0; i < selectedCount; ++i) {
            for (j = 0; j < 3; ++j) {
                if (selected[i]->pointers[j] != 0) {
                    innerOperand = func_002361EC(selected[0]);
                    threshold = func_002363FC(target);
                    innerResult = func_002365BC(selected[0], target,
                                              innerOperand, threshold);
                    randomWord = func_00022e60();
                    randomWord %= 100;
                    hitCount += randomWord < innerResult;
                }
            }
        }
        rounded = (float)(int)((float)func_00235754(selected[0], target) *
                              D_801F68D0[hitCount]);
        /* Retail has a zero guard separate from the one-word result.
         * The guard's original source-level purpose is unknown. */
        result = func_0023697C(selected[0], target, rounded, 0, 0, 0);
        if (auxiliaryGuard != 0)
            auxiliary = func_0022A4E0(target);
        else
            auxiliary = 0;
        func_0022ADFC(target, 0, scheduled[0] + values[0] + work.words[0],
                     result, 0, 0, 1, auxiliary);
        if (endTime < target->field94)
            endTime = target->field94;
    } else {
        followup = (u8)func_00201584(target->field48, target->field4C);
        source = (A9CStateBlock *)(target->pointers[0] + 0x44);
        sourceEnd = (A9CStateBlock *)(target->pointers[0] + 0x94);
        destination = work.fields.state;
        do {
            *destination++ = *source++;
        } while (source != sourceEnd);
        failureState = work.fields.state;
        func_001F0E04(failureState, followup == 0 ? 0x17 : 0x15);
        firstEvaluation = func_001F197C(target, failureState, 0);
        func_0021D230(0xE, scheduled[0] + values[0] + work.words[0], target, 0xFF);
        objectTime = scheduled[0] + values[0] + work.words[0] + firstEvaluation;
        source = (A9CStateBlock *)(target->pointers[0] + 0x44);
        failureSourceEnd = (A9CStateBlock *)(target->pointers[0] + 0x94);
        do {
            *failureState++ = *source++;
        } while (source != failureSourceEnd);
        failureSelector = 0x18;
        failureState = work.fields.state;
        if (followup != 0)
            failureSelector = 0x16;
        func_001F0E64(failureState, failureSelector);
        secondEvaluation = func_001F197C(target, failureState, 0);
        func_0021D230(0xF, objectTime, target, 0xFF);
        firstEvaluation += secondEvaluation;
        if (target->field94 < objectTime + firstEvaluation)
            target->field94 = objectTime + firstEvaluation;
        if (endTime < target->field94)
            endTime = target->field94;
    }
    for (i = 0; i < selectedCount; ++i) {
        secondEvaluation = 0;
        if (func_0020BFE4() != 0) {
            secondEvaluation = func_002223E0(selected[i], target, byte57, byte5B,
                                             selected[i]->field5C, selected[i]->field64);
            func_0021D25C(8, scheduled[i] + values[i] + duration, selected[i], 0xFF, 11);
            func_0021D25C(9, scheduled[i] + values[i] + duration + secondEvaluation,
                         selected[i], 0xFF, 12);
        }
        selected[i]->field84 = values[i];
        selected[i]->field88 = endTime - startTime;
        selected[i]->field8C = duration + secondEvaluation - selected[i]->field88;
        if (selected[i]->field8C < 0)
            selected[i]->field8C = 0;
        selected[i]->field94 = scheduled[i] + values[i] + duration + secondEvaluation;
    }
    *outTime = scheduled[0] + selected[0]->field84 + selected[0]->field88;
    return 1;
}

