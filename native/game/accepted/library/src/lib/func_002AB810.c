typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef signed int s32;
typedef float f32;

/* Bit-preserving compiler view, not a floating-point interpretation of a slot. */
typedef union SlotWord {
    s32 value;
    f32 floatView;
} SlotWord;

/* Local partial views agree with B02's source/unit layout and observed ABI.
 * Unknown scene fields retain offset names rather than stronger semantics. */
typedef struct BodySource {
    u8 pad_00[0x28];
    s16 x, y, z;
} BodySource;

typedef struct BodyUnit {
    BodySource *sources[3];
    u8 pad_0C[0x3C];
    s32 classId;
    s32 variant;
    u8 pad_50[0xA8];
} BodyUnit;

typedef struct SceneBody {
    u8 pad_000[0xE0];
    void *descriptor;
    SlotWord slot;
    s32 source;
    s32 variant;
    s32 sequence;
    s32 sequenceTimer;
    s32 sequenceFlags;
    u8 pad_0FC[0x08];
    f32 scale[3];
    s32 rotate[3];
    f32 x, y, z;
    s32 field_128;
    s32 field_12C;
    f32 field_130;
    s16 state;
    u8 pad_136[0x02];
    s16 animation;
    s16 field_13A;
    u8 field_13C;
    u8 active;
    u8 pad_13E[0x03];
    u8 facing;
    u8 field_142[3];
    u8 field_145;
    u8 field_146;
    u8 field_147;
    u8 pad_148;
    u8 field_149;
    u8 pad_14A[0x06];
} SceneBody;

typedef struct SceneManager {
    u8 pad_00[0x18];
    SceneBody *records[0x1C];
} SceneManager;

extern SceneManager *D_8022A974;
extern s32 D_801CE8FC;
extern u8 D_8022F2CC;
extern s32 func_0020C104(BodyUnit *unit);
extern s32 func_0020C120(BodyUnit *unit);
extern void *resource_alloc(unsigned int size);
extern void func_00023780(void *destination, s32 size);
extern void *func_0002CD70(void *record, s32 arg1, unsigned int arg2);
extern f32 func_001FF7EC(s16 x, s16 z);

static __inline__ void initialize_fields(SceneBody *record, s32 state,
                                        s32 flip, void *descriptor,
                                        s32 fillSize, s32 *sceneWord)
{
    s32 sceneValue;

    record->active = 1;
    sceneValue = *sceneWord;
    record->state = state;
    record->sequence = -1;
    record->sequenceTimer = 0;
    record->sequenceFlags = 0;
    record->field_12C = 0;
    record->descriptor = descriptor;
    record->field_13A = flip;
    record->field_12C = 0;
    record->field_130 = 1.0f;
    record->field_128 = 0;
    record->field_145 = 1;
    record->field_13C = sceneValue;
    func_0002CD70(record, 0xFF, fillSize);
}

static __inline__ void initialize_body(SceneBody *record, s32 state,
                                      s32 classId, s32 variant, s32 flip,
                                      void *descriptor, s32 fillSize, s32 *sceneWord,
                                      s32 kind)
{
    /* Once-only scopes retain KMC flow notes used in register allocation.
     * Their nesting keeps the constants and output cursors in retail homes. */
    do { do { do {
        do {
            if (classId != -1) record->source = classId;
        } while (0);
    } while (0); } while (0); } while (0);
    record->animation = state;
    do {
        do {
            if (variant != -1) record->variant = variant;
        } while (0);
        /* Keep both inline expansions for pre-reload argument scheduling.
         * KMC's late jump pass merges their identical emitted bodies. */
        if (kind) {
            initialize_fields(record, state, flip, descriptor, fillSize, sceneWord);
        } else {
            initialize_fields(record, state, flip, descriptor, fillSize, sceneWord);
        }
        func_00023780((u8 *)record + 0x10, 0x10);
    } while (0);
}

s32 func_002AB810(s32 *slots, BodyUnit *unit, s32 index, s32 flip,
                  s32 classId, s32 variant, u8 seenLeader)
{
    SceneBody *record;
    s32 firstSlot;
    s32 kind;
    s32 i;
    s32 slot;
    s32 state;
    void *descriptor = &D_8022F2CC;
    u8 currentIndex;
    s32 specialKind;
    s32 fillSize;
    SlotWord slotBits;
    s32 *sceneWord;

    firstSlot = 0;
    kind = 0;
    if (func_0020C104(unit) == 1) {
        kind = 1;
    } else if (func_0020C120(unit) == 1) {
        kind = 2;
    } else if (classId == 0x87) {
        if (seenLeader == 1) kind = 4;
    } else if (classId == 0xA1) {
        kind = 2;
    } else if (classId == 0x88) {
        kind = 2;
    }

    sceneWord = &D_801CE8FC;
    fillSize = 0x10;
    i = 0;
    do {
        if (unit->sources[i] != 0) {
            for (slot = 0; slot < 0x1C; slot++) {
                if (D_8022A974->records[slot] == 0) {
                    break;
                }
            }
            slots[i] = slot;
            if (i == 0) {
                firstSlot = slot;
            }
            record = D_8022A974->records[slot] = resource_alloc(0x150);
            func_00023780(record, 0x150);
            if (i == 1 && kind == 2) {
                record->slot.value = slot & 0xFF;
                state = 0x3C;
            } else {
                do { specialKind = 4; } while (0);
                if (kind == specialKind) {
                    /* Preserve the word bits through the alternate union view.
                     * KMC otherwise merges this arm with the earlier state-60 arm.
                     * This is a compiler-shape workaround, not a floating slot. */
                    slotBits.value = slot & 0xFF;
                    record->slot.floatView = slotBits.floatView;
                    state = 0x3C;
                } else {
                    record->slot.value = slot & 0xFF;
                    state = 0xA;
                }
            }
            initialize_body(record, state, classId, variant, flip, descriptor,
                            fillSize, sceneWord, kind);
            record->field_146 = flip;
            record->facing = 0;
            record->x = unit->sources[i]->x;
            record->z = unit->sources[i]->z;
            do { specialKind = 4; } while (0);
            if (kind != specialKind) {
                record->y = (s16)func_001FF7EC(record->x, record->z);
            } else {
                record->y = unit->sources[i]->y;
            }
            for (slot = 0; slot < 3; slot++) {
                record->scale[slot] = 1.0f;
                record->rotate[slot] = 0;
                record->field_142[slot] = 0xFF;
            }
            currentIndex = index;
            record->field_147 = currentIndex;
            record->field_149 = i;
        }
    } while (++i < 3);
    return firstSlot;
}
