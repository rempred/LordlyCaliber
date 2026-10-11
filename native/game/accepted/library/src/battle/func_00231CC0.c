typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct ResolutionObject {
    u8 unknown00[0x2C];
    u16 field2C;
    u8 unknown2E[0x12];
    u32 field40;
    u8 unknown44[0x14];
    int field58;
    u8 unknown5C[0x10];
    int field6C;
    int field70;
} ResolutionObject;

typedef struct ResolutionRecord {
    ResolutionObject *objects[3];
    int field0C;
    int field10;
    u32 unknown14[4];
} ResolutionRecord;

typedef struct ResolutionContext {
    int field00;
} ResolutionContext;

extern ResolutionContext *D_801CE8C0;
extern void func_00023780(void *base, u32 bytes);
extern ResolutionObject *func_0020C478(u32 index);
extern int func_0020C2C0(void *object);

static inline int object_qualified(ResolutionObject *object)
{
    if (object == 0)
        return 0;
    if ((object->field40 & 3) != 0)
        return 0;
    if (object->field6C >= object->field70)
        return 0;
    return object->field6C == D_801CE8C0->field00;
}

void func_00231CC0(ResolutionRecord *records)
{
    ResolutionObject *object;
    ResolutionObject *previous;
    ResolutionRecord temporary;
    int i;
    int slot;
    int below;
    int above;

    func_00023780(records, 6 * sizeof(*records));
    records[0].field10 = 0;
    records[1].field10 = 1;
    records[2].field10 = 2;
    records[3].field10 = 6;
    records[4].field10 = 7;
    records[5].field10 = 8;

    for (i = 0; i < 20; ++i) {
        object = func_0020C478(i);
        if (object == 0 || func_0020C2C0(object) ||
            object->field6C != D_801CE8C0->field00 ||
            object->field6C >= object->field70)
            continue;

        /* The raw slot domain is an upstream precondition of the original path. */
        slot = object->field58;
        if (slot >= 6)
            slot -= 3;

        if (records[slot].objects[0] == 0) {
            records[slot].objects[0] = object;
            continue;
        }
        previous = records[slot].objects[0];
        if (previous->field2C < object->field2C) {
            records[slot].objects[0] = object;
            object = previous;
        }

        if (records[slot].objects[1] == 0) {
            records[slot].objects[1] = object;
            continue;
        }
        previous = records[slot].objects[1];
        if (previous->field2C < object->field2C) {
            records[slot].objects[1] = object;
            object = previous;
        }
        records[slot].objects[2] = object;
    }

    for (i = 0; i < 6; ++i) {
        int score;
        if (object_qualified(records[i].objects[0])) {
            if (object_qualified(records[i].objects[1])) {
                if (object_qualified(records[i].objects[2])) {
                    score = records[i].objects[2]->field2C +
                        ((records[i].objects[1]->field2C + records[i].objects[0]->field2C) >> 1);
                } else {
                    score = records[i].objects[1]->field2C + records[i].objects[0]->field2C;
                }
            } else if (object_qualified(records[i].objects[2])) {
                score = records[i].objects[2]->field2C + records[i].objects[0]->field2C;
            } else {
                score = records[i].objects[0]->field2C;
            }
        } else if (object_qualified(records[i].objects[1])) {
            if (object_qualified(records[i].objects[2])) {
                score = records[i].objects[2]->field2C + records[i].objects[1]->field2C;
            } else {
                score = records[i].objects[1]->field2C;
            }
        } else if (object_qualified(records[i].objects[2])) {
            score = records[i].objects[2]->field2C;
        } else {
            score = 0;
        }
        records[i].field0C = score;
    }

    below = -1;
    above = -1;
    for (i = 0; i < 6; ++i) {
        if (records[i].objects[0] != 0) {
            if (records[i].field10 >= 6) {
                if (above < 0 || records[i].field10 < records[above].field10)
                    above = i;
            } else {
                if (below < 0 || records[below].field10 < records[i].field10)
                    below = i;
            }
        }
    }

    if (above >= 0 && below < 0) {
        i = above;
    } else if (below >= 0 && above < 0) {
        i = below;
    } else if (above >= 0 && below >= 0) {
        /* KMC reverses the two ADD operands for ordinary indexed pointers here.
         * Form the real 36-byte offsets first to retain the original order;
         * both category indices have already been bounded to 0..5. */
        ResolutionRecord *aboveRecord = (ResolutionRecord *)
            ((u32)(above * 0x24) + (u32)records);
        ResolutionRecord *belowRecord = (ResolutionRecord *)
            ((u32)(below * 0x24) + (u32)records);
        int aboveScore = aboveRecord->field0C;
        int belowScore = belowRecord->field0C;
        i = below;
        if (aboveScore >= belowScore)
            i = above;
    } else {
        i = 0;
    }

    while (i != 0) {
        temporary = records[i];
        records[i] = records[i - 1];
        records[i - 1] = temporary;
        --i;
    }
}
