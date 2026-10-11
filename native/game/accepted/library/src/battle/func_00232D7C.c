typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ResolutionObject {
    u8 unknown00[0x94];
    int field94;
} ResolutionObject;

typedef struct ResolutionRecord {
    ResolutionObject *objects[3];
    int field0C;
    int field10;
    u32 unknown14[4];
} ResolutionRecord;

typedef struct ResolutionContext {
    int field00;
    int field04;
    int field08;
    int unknown0C;
    u8 unknown10[0x800];
    int field810;
    int field814;
    u8 field818;
    u8 field819;
    u8 unknown81A[8];
    u8 field822;
    u8 field823;
    u8 unknown824[4];
    u32 field828;
} ResolutionContext;

extern ResolutionContext *D_801CE8C0;
extern u8 *D_801CE8BC;
extern u8 D_801939CC;
extern void func_0021088C(void);
extern void func_00210930(void);
extern void func_00023780(void *, u32);
extern ResolutionObject *func_0020C478(u32);
extern u8 *func_001F21A4(int);
extern int func_0020BF7C(void);
/* The getter produces a full zero-extended word, as declared by its existing callers. */
extern int func_0020BF8C(void);
extern int func_0020F8DC(void);
extern int func_0020F80C(void);
extern int func_002335E0(int);
extern int func_0023370C(void);
extern void func_00231CC0(ResolutionRecord *);
extern void func_00232280(ResolutionRecord *);
extern int func_00232458(u8 *, int *);
extern int func_002317C8(int *, int);
extern int func_00230A9C(ResolutionRecord *, int *);
extern int func_0022F580(ResolutionRecord *, int *);
extern int func_0022F2BC(void *, int *);
extern void func_0021D1CC(int, int);

/*
 * An inline parameter keeps these two output addresses as direct call arguments.
 * A separate early address invariant changes LOOP hoisting and the saved frame.
 */
static inline void resolve_side(int *outTime, int side)
{
    func_002317C8(outTime, side);
}

void func_00232D7C(void)
{
    struct {
        ResolutionRecord records[6];
        int outTime;
    } work;
    ResolutionRecord selected;
    ResolutionObject *object;
    ResolutionContext *initialContext;
    ResolutionContext *finalContext;
    ResolutionContext *clearContext;
    u8 *global;
    u8 *descriptor;
    int i;
    int maximum;
    int stopped;
    int oldRetry;
    int empty;
    int retries;
    u16 descriptorIndex;
    u32 globalTime;
    u32 finalTime;
    /* Sharing the count/time scalar preserves its saved register across these phases. */
    int savedValue;

    func_0021088C();
    D_801CE8C0->field823 = 0;
    D_801CE8C0->field822 = 0;
    initialContext = D_801CE8C0;
    initialContext->field818 = 0;
    initialContext->field819 = 0;
    global = D_801CE8BC;
    initialContext->field814 = 0;
    initialContext->field828 = 0;
    initialContext->field00 = 0;
    global[0x6088] = 0;
    clearContext = D_801CE8C0;
    *(u16 *)(global + 0x606A) = 0;
    func_00023780(clearContext->unknown10, 0x800);
    for (i = 0; i < 20; ++i) {
        object = func_0020C478(i);
        if (object != 0)
            object->field94 = 0;
    }

    descriptorIndex = *(u16 *)(D_801CE8BC + 0x6076);
    work.outTime = 0;
    if (descriptorIndex != 0) {
        descriptor = func_001F21A4(descriptorIndex & 0xFF);
        /* The original has no local bit-index guard; its byte domain is upstream. */
        D_801939CC &= ~(1u << descriptor[0]);
        func_00232458(descriptor, &work.outTime);
        *(u16 *)(D_801CE8BC + 0x6076) = 0;
    }

    for (;;) {
        if (D_801CE8BC[0x6087] != 0 || func_0020F8DC() != 0)
            break;
        func_00231CC0(work.records);
        func_00232280(work.records);
        stopped = 0;
        retries = 3;
        do {
            if (work.records[0].objects[0] != 0) {
                empty = work.records[0].objects[0] == 0;
                goto record_ready;
            }
            D_801CE8C0->field00++;
            {
                int firstQuantity;
                int secondQuantity;
                int side;
                int complete;
                if (func_0020BF8C() == 0) {
                    if (func_0020BF7C() == 0) {
                        firstQuantity = func_002335E0(0);
                        secondQuantity = func_002335E0(1);
                        savedValue = (firstQuantity != 0) + (secondQuantity != 0);
                    } else {
                        savedValue = 0;
                    }
                } else {
                    savedValue = 0;
                }
                if (savedValue == 0) {
                    complete = 0;
                    goto missing_head_checked;
                }
                side = func_0023370C();
                resolve_side(&work.outTime, side);
                if (func_0020F8DC() != 0) {
                    complete = 1;
                    goto missing_head_checked;
                }
                if (savedValue == 2) {
                    resolve_side(&work.outTime, side == 0);
                    if (func_0020F8DC() != 0) {
                        complete = 1;
                        goto missing_head_checked;
                    }
                }
                complete = 0;
missing_head_checked:
                if (complete) {
                    stopped = 1;
                    break;
                }
            }
            func_00231CC0(work.records);
            /* Test before decrement: the missing-head path may run four times. */
            oldRetry = retries;
            --retries;
        } while (oldRetry);
        empty = work.records[0].objects[0] == 0;
record_ready:
        if (empty | stopped)
            break;
        selected = work.records[0];
        if (func_00230A9C(&selected, &work.outTime) != 0 ||
            func_0022F580(&selected, &work.outTime) != 0) {
            if (func_0020F8DC() != 0)
                break;
            continue;
        }
        if (selected.objects[0] == 0)
            continue;
        func_0022F2BC(selected.objects[0], &work.outTime);
        if (func_0020F8DC() != 0)
            break;
        if (selected.objects[1] == 0)
            continue;
        func_0022F2BC(selected.objects[1], &work.outTime);
        if (func_0020F8DC() != 0)
            break;
        if (selected.objects[2] == 0)
            continue;
        func_0022F2BC(selected.objects[2], &work.outTime);
        if (func_0020F8DC() != 0)
            break;
    }

    maximum = 0;
    for (i = 0; i < 20; ++i) {
        object = func_0020C478(i);
        if (object != 0 && maximum < object->field94)
            maximum = object->field94;
        finalTime = maximum;
    }
    globalTime = D_801CE8C0->field828;
    if (globalTime >= finalTime)
        finalTime = globalTime;
    savedValue = finalTime;
    D_801CE8BC[0x6086] = 0;
    if ((func_0020BF7C() != 0 || func_0020BF8C() != 0) &&
        (func_0020F80C() != 0 || D_801CE8BC[0x6087] != 0))
        D_801CE8BC[0x6086] = 1;
    func_0021D1CC(0x41, (u32)savedValue + 20);
    func_00210930();
    finalContext = D_801CE8C0;
    finalContext->field823 = 0;
    finalContext->field810 = 0;
    finalContext->field04 = 0;
    finalContext->field08 = 0;
    D_801CE8C0->field822 = 0;
    D_801CE8C0->field00 = 0;
}
