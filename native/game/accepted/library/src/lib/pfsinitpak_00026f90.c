typedef struct {
    unsigned int field00;
    void *queue;
    int channel;
    unsigned char field0c[32];
    unsigned char field2c[32];
    int field4c;
    int field50;
    int field54;
    int field58;
    unsigned int field5c;
    int field60;
    unsigned char field64;
    unsigned char field65;
} PfsState;

typedef struct {
    unsigned char field00[24];
    unsigned short field18;
    unsigned char field1a;
    unsigned char field1b;
    unsigned short field1c;
    unsigned short field1e;
} PakHeader;

extern void func_00024bc4(void);
extern void func_00024c30(void);
extern int func_000272c0(void *queue, int channel);
extern int func_000274d0(PfsState *state, unsigned char bank);
extern int func_00027fd0(void *queue, int channel, unsigned short address,
                        unsigned char *data);
extern int func_000281c0(void *queue, int channel, unsigned short address,
                        unsigned char *data, int mode);
extern int func_00027574(PakHeader *header, unsigned short *sum,
                         unsigned short *inverse);
extern int func_000278a4(PfsState *state, PakHeader *header);
extern int func_000275b8(PfsState *state, PakHeader *oldHeader,
                        PakHeader *newHeader);
extern void func_80093060(const void *source, void *destination, unsigned int bytes);
extern int func_00023350(const void *first, const void *second, unsigned int bytes);
extern int func_0002b070(PfsState *state);
int func_00027188(PfsState *state);

int func_00026f90(void *queue, PfsState *state, int channel)
{
    PakHeader header;
    PakHeader repaired;
    unsigned short sum;
    unsigned short inverse;
    PakHeader *active;
    int result;

    func_00024bc4();
    result = func_000272c0(queue, channel);
    func_00024c30();
    if (result != 0)
        return result;
    state->queue = queue;
    state->channel = channel;
    state->field00 = 0;
    result = func_00027188(state);
    if (result != 0)
        return result;
    result = func_000274d0(state, 0);
    if (result != 0)
        return result;
    result = func_00027fd0(state->queue, state->channel, 1,
                          (unsigned char *)&header);
    if (result != 0)
        return result;
    func_00027574(&header, &sum, &inverse);
    active = &header;
    if (header.field1c != sum || header.field1e != inverse) {
        result = func_000278a4(state, active);
        if (result != 0) {
            state->field00 |= 4;
            return result;
        }
    }
    if (!(active->field18 & 1)) {
        result = func_000275b8(state, active, &repaired);
        if (result != 0) {
            if (result == 10)
                state->field00 |= 4;
            return result;
        }
        /* Select the repaired header only on the successful path. This keeps
         * its address live across the status check in the pinned compiler. */
        active = &repaired;
        if (!(active->field18 & 1))
            return 11;
    }
    func_80093060(active, state->field0c, 32);
    state->field4c = active->field1b;
    state->field64 = active->field1a;
    /* Keep the first bank-byte use adjacent: intervening word stores cause
     * the pinned compiler to reload it and alter the retail schedule. */
    state->field60 = state->field64 * 2 + 3;
    state->field50 = 16;
    state->field54 = 8;
    state->field58 = (state->field64 + 1) * 8;
    state->field5c = state->field58 + state->field64 * 8;
    result = func_00027fd0(state->queue, state->channel, 7, state->field2c);
    if (result != 0)
        return result;
    result = func_0002b070(state);
    state->field00 |= 1;
    return result;
}

int func_00027188(PfsState *state)
{
    unsigned char test[32];
    unsigned char readback[32];
    unsigned char saved[32];
    int i;
    int result;

    result = func_000274d0(state, 0);
    if (result != 0)
        return result;
    result = func_00027fd0(state->queue, state->channel, 0, saved);
    if (result != 0)
        return result;
    for (i = 31; i >= 0; i--)
        test[i] = i;
    result = func_000281c0(state->queue, state->channel, 0, test, 0);
    if (result != 0)
        return result;
    result = func_00027fd0(state->queue, state->channel, 0, readback);
    if (result != 0)
        return result;
    if (func_00023350(test, readback, 32) != 0)
        return 11;
    return func_000281c0(state->queue, state->channel, 0, saved, 0);
}
