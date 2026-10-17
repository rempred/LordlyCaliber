typedef struct {
    unsigned int field00;
    void *queue;
    int channel;
    unsigned char field0c[0x44];
    int field50;
    unsigned char field54[8];
    unsigned int field5c;
    int field60;
    unsigned char field64;
    unsigned char field65;
} PfsState;

typedef union {
    unsigned short value;
    struct { unsigned char byte00, byte01; } bytes;
} Node;

typedef struct {
    unsigned int field00;
    unsigned short field04;
    Node field06;
    unsigned char field08[2];
    unsigned short field0a;
    unsigned char field0c[4];
    unsigned char field10[16];
} DirectoryEntry;

typedef struct {
    Node table[128];
    unsigned char field100;
    unsigned char field101[256];
} CheckerState;

extern int func_00027bd4(PfsState *state);
extern int func_00027a08(PfsState *state);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027c8c(PfsState *state, Node *table,
                        unsigned char mode, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                        unsigned char *data);
extern int func_000281c0(void *queue, int channel, unsigned short address,
                        unsigned char *data, int mode);
extern void func_00023780(void *data, unsigned int bytes);
int func_0002b070(PfsState *state);
int func_0002b4d0(PfsState *state, CheckerState *scratch);
int func_0002b628(PfsState *state, Node node, CheckerState *scratch);

int func_0002b070(PfsState *state)
{
    Node table[128];
    Node previous[128];
    DirectoryEntry entry;
    Node starts[16];
    CheckerState scratch;
    Node node;
    int slot;
    int start;
    int result;
    int mismatch;
    int expected;
    int changed = 0;
    unsigned char previousBank = 254;
    unsigned char bank;

    result = func_00027bd4(state);
    if (result == 2)
        result = func_00027a08(state);
    if (result != 0)
        return result;
    result = func_0002b4d0(state, &scratch);
    if (result != 0)
        return result;
    for (slot = 0; slot < state->field50; slot++) {
        result = func_00027fd0(state->queue, state->channel,
                               state->field5c + slot,
                               (unsigned char *)&entry);
        if (result != 0)
            return result;
        if (entry.field04 == 0) {
            mismatch = -1;
            if (entry.field00 == 0)
                continue;
        } else {
            mismatch = 0;
            if (entry.field00 == 0) {
                mismatch = -1;
            } else {
                node = entry.field06;
                expected = 0;
                bank = 255;
                while (node.value >= state->field60 &&
                       node.bytes.byte00 < state->field64 &&
                       ((node.bytes.byte01 > 0) & (node.bytes.byte01 < 128))) {
                    if (bank != node.bytes.byte00) {
                        bank = node.bytes.byte00;
                        if (previousBank != node.bytes.byte00) {
                            result = func_00027c8c(state, previous, 0,
                                                   node.bytes.byte00);
                            previousBank = bank;
                        }
                        if ((result != 0) & (result != 3))
                            return result;
                    }
                    mismatch = func_0002b628(state, node, &scratch) - expected;
                    if (mismatch != 0)
                        break;
                    node = previous[node.bytes.byte01];
                    expected = 1;
                }
            }
        }
        if (mismatch != 0 || node.value != 1) {
            func_00023780(&entry, 32);
            if (state->field65 != 0) {
                result = func_000274d0(state, 0);
                if (result != 0)
                    return result;
            }
            result = func_000281c0(state->queue, state->channel,
                                   state->field5c + slot,
                                   (unsigned char *)&entry, 0);
            if (result != 0)
                return result;
            changed++;
        }
    }
    for (slot = 0; slot < state->field50; slot++) {
        result = func_00027fd0(state->queue, state->channel,
                               state->field5c + slot,
                               (unsigned char *)&entry);
        if (result != 0)
            return result;
        if (entry.field04 != 0 && entry.field00 != 0 &&
            entry.field06.value >= (unsigned short)state->field60)
            starts[slot].value = entry.field06.value;
        else
            starts[slot].value = 0;
    }
    for (bank = 0; bank < state->field64; bank++) {
        result = func_00027c8c(state, previous, 0, bank);
        if ((result != 0) & (result != 3))
            return result;
        start = bank == 0 ? state->field60 : 1;
        for (slot = 0; slot < start; slot++)
            table[slot].value = previous[slot].value;
        for (; slot < 128; slot++)
            table[slot].value = 3;
        for (slot = 0; slot < state->field50; slot++) {
            while (starts[slot].bytes.byte00 == bank) {
                Node next;
                if (starts[slot].value < (unsigned short)state->field60)
                    break;
                /* Snapshot once before either destination is updated. */
                next = previous[starts[slot].bytes.byte01];
                table[starts[slot].bytes.byte01] = next;
                starts[slot] = next;
            }
        }
        result = func_00027c8c(state, table, 1, bank);
        if (result != 0)
            return result;
    }
    if (changed != 0)
        state->field00 |= 2;
    else
        state->field00 &= ~2U;
    return 0;
}

int func_0002b4d0(PfsState *state, CheckerState *scratch)
{
    Node table[128];
    int i;
    int start;
    /* Widen across the call, then use its byte view in the scan. */
    int bankIndex;
    int mapIndex;
    int result;
    unsigned char bank;
    unsigned short value;
    unsigned int clearAddress;

    /* A 32-bit address cursor keeps the known field displacement on
     * each byte store with the pinned compiler. */
    i = 255;
    clearAddress = (unsigned int)scratch + i;
    for (; i >= 0; i--) {
        *(unsigned char *)(clearAddress + sizeof(scratch->table) +
                            sizeof(scratch->field100)) = 0;
        clearAddress--;
    }
    scratch->field100 = 255;
    for (bank = 0; bank < state->field64; bank++) {
        bankIndex = bank;
        start = bankIndex == 0 ? state->field60 : 1;
        result = func_00027c8c(state, table, 0, bankIndex);
        if ((result != 0) & (result != 3))
            return result;
        for (i = start; i < 128; i++) {
            value = table[i].value;
            if (value >= state->field60 && (value >> 8) != (unsigned char)bankIndex) {
                /* Preserve the two-step index accumulation order. */
                mapIndex = (value & 127) >> 2;
                mapIndex += ((value >> 8) & 7) * 32;
                scratch->field101[mapIndex] |= 1 << (bankIndex & 7);
            }
        }
    }
    return 0;
}

/* The aggregate parameter lets the pinned compiler emit the entry shift. */
int func_0002b628(PfsState *state, Node node, CheckerState *scratch)
{
    unsigned char bank;
    int count = 0;
    /* Share the widened pre-call value; keep the byte counter for the store. */
    int bankIndex;
    int i;
    int start;
    int result;
    int mapIndex = (node.bytes.byte01 >> 2) +
                   ((node.bytes.byte00 & 7) * 32);

    for (bank = 0; bank < state->field64; bank++) {
        bankIndex = bank;
        start = bankIndex == 0 ? state->field60 : 1;
        if (bankIndex == node.bytes.byte00 ||
            ((scratch->field101[mapIndex] >> (bankIndex & 7)) & 1)) {
            if (bankIndex != scratch->field100) {
                result = func_00027c8c(state, scratch->table, 0, bankIndex);
                if ((result != 0) & (result != 3))
                    return result;
                scratch->field100 = bank;
            }
            for (i = start; (count < 2) & (i < 128); i++)
                count += scratch->table[i].value == node.value;
            if (count >= 2)
                return 2;
        }
    }
    return count;
}
