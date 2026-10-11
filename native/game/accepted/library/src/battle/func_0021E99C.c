typedef struct TerminalCopyActor {
    unsigned char prefix[0x5C];
    unsigned int word5C;
    unsigned int word60;
    unsigned int word64;
} TerminalCopyActor;

typedef struct TerminalCopyState {
    int word00;
    unsigned int scaledX;
    unsigned int scaledY;
    unsigned int scaledZ;
    unsigned char gap10[0xC];
    short currentX;
    short currentY;
    short currentZ;
    unsigned char gap22[6];
    short destinationX;
    short destinationY;
    short destinationZ;
    unsigned char gap2E[0x12];
    TerminalCopyActor *actor;
} TerminalCopyState;

int func_0020C104(void *actor);
int func_0020C120(void *actor);

void func_0021E99C(TerminalCopyState *state, TerminalCopyState *second)
{
    unsigned short x;
    unsigned short y;
    unsigned short z;
    TerminalCopyActor *actor;

    /* Keep the original copy order for possibly overlapping state objects. */
    x = state->destinationX;
    second->currentX = x;
    y = state->destinationY;
    state->currentX = x;
    second->currentY = y;
    z = state->destinationZ;
    state->currentY = y;
    second->currentZ = z;
    state->currentZ = z;

    if (func_0020C104(state->actor) != 0 || func_0020C120(state->actor) != 0) {
        actor = state->actor;
        state->scaledX = actor->word5C * 30u - (actor->word5C - state->destinationX);
        state->scaledY = actor->word60 * 30u - (actor->word60 - state->destinationY);
        state->scaledZ = actor->word64 * 30u - (actor->word64 - state->destinationZ);
    } else {
        state->scaledX = state->destinationX * 30;
        state->scaledY = state->destinationY * 30;
        state->scaledZ = state->destinationZ * 30;
    }
}
