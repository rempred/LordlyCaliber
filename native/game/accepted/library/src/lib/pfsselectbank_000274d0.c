typedef struct {
    unsigned int field00;
    void *queue;
    int channel;
    unsigned char field0c[0x59];
    unsigned char field65;
} PfsState;

extern int func_000281c0(void *queue, int channel, unsigned short address,
                       unsigned char *data, int force);

int func_000274d0(PfsState *state, unsigned char bank)
{
    unsigned char data[32];
    int result;
    int i;

    for (i = 31; i >= 0; i--)
        data[i] = bank;
    result = func_000281c0(state->queue, state->channel, 0x400, data, 0);
    if (result == 0)
        state->field65 = bank;
    return result;
}
