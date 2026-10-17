typedef struct {
    unsigned int field00;
    void *queue;
    int channel;
    unsigned char field0c[0x44];
    int field50;
    unsigned char field54[8];
    unsigned int field5c;
    unsigned char field60[5];
    unsigned char field65;
} PfsState;

typedef struct {
    unsigned int field00;
    unsigned short field04;
    unsigned char field06[26];
} DirectoryEntry;

extern int func_00027bd4(PfsState *state);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                         unsigned char *data);
extern int func_000272c0(void *queue, int channel);

int func_00026e90(PfsState *state, int *maxFiles, int *usedFiles)
{
    DirectoryEntry entry;
    int result;
    int used = 0;
    int i;

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
    for (i = 0; i < state->field50; i++) {
        result = func_00027fd0(state->queue, state->channel,
                              (unsigned short)(state->field5c + i),
                              (unsigned char *)&entry);
        if (result != 0)
            return result;
        if (entry.field04 != 0 && entry.field00 != 0)
            used++;
    }
    *usedFiles = used;
    *maxFiles = state->field50;
    return func_000272c0(state->queue, state->channel);
}
