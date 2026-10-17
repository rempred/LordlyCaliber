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
    unsigned char field08[2];
    unsigned short field0a;
    unsigned char field0c[4];
    unsigned char field10[16];
} DirectoryEntry;

extern int func_00026bc0(PfsState *state, unsigned short field04,
                        unsigned int field00, const unsigned char *field10,
                        const unsigned char *field0c, int *file);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                        unsigned char *data);
extern int func_000281c0(void *queue, int channel, unsigned short address,
                        unsigned char *data, int mode);
extern int func_00027c8c(PfsState *state, Node *table, unsigned char mode,
                        unsigned char bank);
extern void func_80093380(void *destination, unsigned int bytes);
int func_0002656c(PfsState *state, Node *table, int page,
                  int bank, Node *next);

int func_000263c0(PfsState *state, unsigned short field04,
                  unsigned int field00, const unsigned char *field10,
                  const unsigned char *field0c)
{
    Node table[128];
    DirectoryEntry entry;
    int file;
    Node next;
    unsigned char bank;
    unsigned char page;
    int result;

    if (field04 == 0 || field00 == 0)
        return 5;
    result = func_00026bc0(state, field04, field00, field10, field0c, &file);
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
    bank = entry.field06.bytes.byte00;
    page = entry.field06.bytes.byte01;
    while (bank < state->field64) {
        result = func_00027c8c(state, table, 0, bank);
        if (result != 0)
            return result;
        result = func_0002656c(state, table, page, bank, &next);
        if (result != 0)
            return result;
        result = func_00027c8c(state, table, 1, bank);
        if (result != 0)
            return result;
        if (next.value == 1)
            break;
        bank = next.bytes.byte00;
        page = next.bytes.byte01;
    }
    if (bank < state->field64) {
        func_80093380(&entry, 32);
        result = func_000281c0(state->queue, state->channel,
                               (unsigned short)(state->field5c + file),
                               (unsigned char *)&entry, 0);
    } else {
        result = 3;
    }
    return result;
}

int func_0002656c(PfsState *state, Node *table, int page,
                  int bank, Node *next)
{
    unsigned short current;
    unsigned short previous;

    /* Scalar links avoid unused union stack slots in the pinned compiler.
     * Keep the old link until its table entry has been cleared. */
    current = ((unsigned int)bank << 8) | (page & 0xff);
    do {
        Node *link = table;
        previous = current;
        current = table[current & 0xff].value;
        link += previous & 0xff;
        link->value = 3;
    } while (current >= state->field60 && (current >> 8) == (unsigned char)bank);
    next->value = current;
    return 0;
}
