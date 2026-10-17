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
extern int func_00026d80(PfsState *state, int *bytes);
extern int func_00027c8c(PfsState *state, Node *table, unsigned char mode,
                        unsigned char bank);
extern void func_00023460(const void *source, void *destination,
                         unsigned int bytes);
extern int func_000281c0(void *queue, int channel, unsigned short address,
                        unsigned char *data, int mode);
int func_0002629c(PfsState *state, Node *table, int pages, int *first,
                  unsigned char bank, int *count, int *last);

int func_00025f90(PfsState *state, unsigned short field04,
                  unsigned int field00, const unsigned char *field10,
                  const unsigned char *field0c, int size, int *file)
{
    Node table[128];
    Node previous[128];
    DirectoryEntry entry;
    int freeBytes;
    int first;
    int count;
    int last;
    int result;
    int pages;
    unsigned char bank;
    int previousLast = 0;
    unsigned char previousBank = 0;
    int havePrevious = 0;
    Node firstNode;

    if (field04 == 0 || field00 == 0)
        return 5;
    pages = (size + 255) / 256;
    result = func_00026bc0(state, field04, field00, field10, field0c, file);
    if (result != 0) {
        if (result != 5)
            return result;
    }
    if (*file != -1)
        return 9;
    func_00026d80(state, &freeBytes);
    if (freeBytes < size)
        return 7;
    if (pages == 0)
        return 5;
    result = func_00026bc0(state, 0, 0, 0, 0, file);
    if (result != 0) {
        if (result != 5)
            return result;
    }
    if (*file == -1)
        return 8;
    for (bank = 0; bank < state->field64; bank++) {
        result = func_00027c8c(state, table, 0, bank);
        if (result != 0)
            return result;
        result = func_0002629c(state, table, pages, &first, bank,
                              &count, &last);
        if (result != 0)
            return result;
        if (first != -1) {
            if (havePrevious) {
                previous[previousLast].bytes.byte00 = bank;
                previous[previousLast].bytes.byte01 = first;
                result = func_00027c8c(state, previous, 1, previousBank);
                if (result != 0)
                    return result;
            } else {
                /* Separate byte updates preserve KMC's first-node lifetime.
                 * Packing a scalar instead spills this value. */
                firstNode.bytes.byte00 = bank;
                firstNode.bytes.byte01 = first;
            }
            if (count >= pages) {
                pages = 0;
                result = func_00027c8c(state, table, 1, bank);
                if (result != 0)
                    return result;
                break;
            }
            func_00023460(table, previous, 256);
            previousLast = last;
            previousBank = bank;
            havePrevious++;
            pages -= count;
        }
    }
    if (pages > 0 || first == -1)
        return 3;
    entry.field04 = field04;
    entry.field00 = field00;
    entry.field06.value = firstNode.value;
    entry.field0a = 0;
    func_00023460(field10, entry.field10, 16);
    func_00023460(field0c, entry.field0c, 4);
    return func_000281c0(state->queue, state->channel,
                         (unsigned short)(state->field5c + *file),
                         (unsigned char *)&entry, 0);
}

int func_0002629c(PfsState *state, Node *table, int pages, int *first,
                  unsigned char bank, int *count, int *last)
{
    int start;
    int i;
    int current;
    int firstPage;
    int result;

    start = 1;
    result = 0;
    if (bank == 0)
        start = state->field60;
    for (i = start; i < 128; i++) {
        if (table[i].value == 3)
            break;
    }
    if (i == 128) {
        *first = -1;
        return result;
    } else {
        *count = 1;
        /* KMC gives the retail register order to this shared snapshot. */
        current = firstPage = i;
        for (i = current + 1; *count < pages && i < 128; i++) {
            if (table[i].value == 3) {
                /* A named write pointer preserves native operand order. */
                Node *link = table;
                link += current;
                link->bytes.byte00 = bank;
                link->bytes.byte01 = i;
                current = i;
                (*count)++;
            }
        }
        *first = firstPage;
        if (i == 128 && *count < pages) {
            *last = current;
        } else {
            Node *link = table;
            link += current;
            link->value = 1;
            *last = 0;
        }
    }
    return result;
}
