typedef struct {
    unsigned int field00;
    void *queue;
    int channel;
    unsigned char field0c[0x54];
    int field60;
    unsigned char field64;
} PfsState;

extern int func_00027bd4(PfsState *state);
extern int func_00027c8c(PfsState *state, unsigned short *table, unsigned char mode,
                         unsigned char bank);

int func_00026d80(PfsState *state, int *freeBytes)
{
    unsigned short table[128];
    int result;
    int count = 0;
    unsigned char bank;
    int block;
    int start;

    if (!(state->field00 & 1))
        return 5;
    result = func_00027bd4(state);
    if (result != 0)
        return result;
    for (bank = 0; bank < state->field64; bank++) {
        result = func_00027c8c(state, table, 0, bank);
        if (result != 0)
            return result;
        start = bank == 0 ? state->field60 : 1;
        for (block = start; block < 128; block++) {
            if (table[block] == 3)
                count++;
        }
    }
    *freeBytes = count << 8;
    return 0;
}
