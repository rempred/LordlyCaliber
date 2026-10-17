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

typedef struct {
    unsigned int field00;
    unsigned short field04;
    unsigned short field06;
    unsigned char field08[4];
    unsigned char field0c[4];
    unsigned char field10[16];
} DirectoryEntry;

typedef struct {
    unsigned int field00;
    unsigned int field04;
    unsigned short field08;
    unsigned char field0a[4];
    unsigned char field0e[16];
} FileState;

typedef union {
    unsigned short value;
    struct {
        unsigned char byte00;
        unsigned char byte01;
    } bytes;
} Node;

extern int func_00027bd4(PfsState *state);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                         unsigned char *data);
extern int func_00027c8c(PfsState *state, unsigned short *table, unsigned char mode,
                         unsigned char bank);
extern void func_00023460(const void *source, void *destination,
                          unsigned int bytes);
extern int func_000272c0(void *queue, int channel);

int func_00026a20(PfsState *state, int file, FileState *output)
{
    unsigned short table[128];
    DirectoryEntry entry;
    int result;
    int count;
    Node node;
    unsigned char bank;

    if (file >= state->field50 || file < 0)
        return 5;
    if (!(state->field00 & 1))
        return 5;
    result = func_00027bd4(state);
    if (result != 0)
        return result;
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
    count = 0;
    node.value = entry.field06;
    bank = 0xff;
    /* Keep the exit check at the head of the traversal. */
    for (;;) {
        if (node.value < state->field60)
            goto traversal_done;
        if (node.bytes.byte00 != bank) {
            bank = node.bytes.byte00;
            result = func_00027c8c(state, table, 0, bank);
            if (result != 0)
                return result;
        }
        node.value = table[node.bytes.byte01];
        count++;
    }
traversal_done:
    if (node.value != 1)
        return 3;
    output->field00 = count << 8;
    output->field08 = entry.field04;
    output->field04 = entry.field00;
    func_00023460(entry.field10, output->field0e, 16);
    func_00023460(entry.field0c, output->field0a, 4);
    return func_000272c0(state->queue, state->channel);
}
