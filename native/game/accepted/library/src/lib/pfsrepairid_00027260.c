typedef struct {
    unsigned int field00;
} PfsState;

extern int func_00027a08(PfsState *state);

int func_00027260(PfsState *state)
{
    int result;

    if (state->field00 & 5) {
        result = func_00027a08(state);
        if (result == 0)
            state->field00 &= ~4;
    } else
        result = 5;
    return result;
}
