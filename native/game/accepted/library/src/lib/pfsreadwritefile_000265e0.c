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
    struct {
        unsigned char byte00;
        unsigned char byte01;
    } bytes;
} Node;

typedef struct {
    unsigned int field00;
    unsigned short field04;
    Node field06;
    unsigned char field08;
    unsigned char field09[3];
    unsigned char field0c[4];
    unsigned char field10[16];
} DirectoryEntry;

extern int func_00027bd4(PfsState *state);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                         unsigned char *data);
extern int func_000281c0(void *queue, int channel, unsigned short address,
                         unsigned char *data, int mode);
extern int func_00027c8c(PfsState *state, unsigned short *table, unsigned char mode,
                         unsigned char bank);
extern int func_000272c0(void *queue, int channel);

#define VALID_NODE(s, n) ((n)->value >= (s)->field60 && \
                         (n)->bytes.byte00 < (s)->field64 && \
                         (n)->bytes.byte01 != 0 && (n)->bytes.byte01 < 128)

static inline int next_node(PfsState *state, unsigned short *table,
                            unsigned char *bank, Node *node)
{
    int result;

    if (node->bytes.byte00 != *bank) {
        *bank = node->bytes.byte00;
        result = func_00027c8c(state, table, 0, *bank);
        if (result != 0)
            return result;
    }
    node->value = table[node->bytes.byte01];
    if (!VALID_NODE(state, node)) {
        if (node->value == 1)
            return 5;
        return 3;
    }
    return 0;
}

int func_000265e0(PfsState *state, int file, unsigned char mode,
                  int offset, int size, unsigned char *data)
{
    DirectoryEntry entry;
    unsigned short table[128];
    unsigned char bank;
    Node node;
    int result;
    int blocks;
    unsigned char *cursor;
    int address;
    int blockOffset;

    if (file >= state->field50 || file < 0)
        return 5;
    if (size <= 0 || (size & 31))
        return 5;
    if (offset < 0 || (offset & 31))
        return 5;
    if (!(state->field00 & 1))
        return 5;
    if (func_00027bd4(state) == 2)
        return 2;
    if (state->field65 != 0) {
        result = func_000274d0(state, 0);
        if (result != 0)
            return result;
    }
    result = func_00027fd0(state->queue, state->channel,
                          (unsigned short)(state->field5c + file),
                          (unsigned char *)&entry);
    if (result != 0)
        return result;
    if (entry.field04 == 0 || entry.field00 == 0)
        return 5;
    if (!VALID_NODE(state, &entry.field06)) {
        if (entry.field06.value == 1)
            return 5;
        return 3;
    }
    if (mode == 0 && !(entry.field08 & 2))
        return 6;
    bank = 0xff;
    blockOffset = offset / 32;
    node = entry.field06;
    while (blockOffset >= 8) {
        result = next_node(state, table, &bank, &node);
        if (result != 0)
            return result;
        blockOffset -= 8;
    }
    blocks = size / 32;
    cursor = data;
    for (; blocks > 0; blocks--) {
        if (blockOffset == 8) {
            result = next_node(state, table, &bank, &node);
            if (result != 0)
                return result;
            blockOffset = 0;
        }
        if (state->field65 != node.bytes.byte00) {
            result = func_000274d0(state, node.bytes.byte00);
            if (result != 0)
                return result;
        }
        address = node.bytes.byte01 * 8 + blockOffset;
        if (mode == 0)
            result = func_00027fd0(state->queue, state->channel,
                                  (unsigned short)address, cursor);
        else
            result = func_000281c0(state->queue, state->channel,
                                  (unsigned short)address, cursor, 0);
        if (result != 0)
            return result;
        cursor += 32;
        blockOffset++;
    }
    if (mode == 1 && !(entry.field08 & 2)) {
        entry.field08 |= 2;
        if (state->field65 != 0) {
            result = func_000274d0(state, 0);
            if (result != 0)
                return result;
        }
        result = func_000281c0(state->queue, state->channel,
                              (unsigned short)(state->field5c + file),
                              (unsigned char *)&entry, 0);
        if (result != 0)
            return result;
    }
    return func_000272c0(state->queue, state->channel);
}
