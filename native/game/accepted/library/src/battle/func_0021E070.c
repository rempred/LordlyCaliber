typedef struct TerminalMotionActor {
    unsigned char prefix[0x48];
    int sourceArt;
    int ownerContext;
    unsigned char gap50[0xC];
    unsigned int word5C;
    unsigned int word60;
    unsigned int word64;
} TerminalMotionActor;

typedef struct TerminalMotionState {
    unsigned char prefix[0x1C];
    short currentX;
    short currentY;
    short currentZ;
    unsigned char gap22[0x1E];
    TerminalMotionActor *actor;
    unsigned char gap44[0x54];
    float destinationX;
    float destinationZ;
    float startX;
    float startZ;
    float phaseX;
    float phaseZ;
    float incrementX;
    float incrementZ;
} TerminalMotionState;

float func_00201670(int sourceArt, int ownerContext);
float func_0002d3e0(float z, float x);
double cos(double angle);
double sin(double angle);

void func_0021E070(TerminalMotionState *state, TerminalMotionActor *target)
{
    float dx;
    float dz;
    float speed;
    float angle;
    float incrementX;
    float incrementZ;

    /* The integer destination calculation precedes both float conversions. */
    state->destinationX = (float)(int)(target->word5C -
        (state->actor->word5C - state->currentX));
    state->destinationZ = (float)(int)(target->word64 -
        (state->actor->word64 - state->currentZ));
    dx = state->destinationX - (float)state->currentX;
    dz = state->destinationZ - (float)state->currentZ;
    speed = func_00201670(state->actor->sourceArt, state->actor->ownerContext);
    incrementX = dx;
    if (dx != 0.0f) {
        angle = func_0002d3e0(dz, dx);
        /* Retail repeats each trig call. Narrow after the double conditional;
         * separate float assignments add eight bytes with the pinned GCC. */
        incrementX = (float)((cos((double)angle) * (double)speed > 0.0)
            ? cos((double)angle) * (double)speed
            : -(cos((double)angle) * (double)speed));
        incrementZ = (float)((sin((double)angle) * (double)speed > 0.0)
            ? sin((double)angle) * (double)speed
            : -(sin((double)angle) * (double)speed));
    } else {
        incrementZ = speed;
    }
    state->incrementX = incrementX;
    state->incrementZ = incrementZ;
    if (dx < 0.0f) {
        state->incrementX = -state->incrementX;
    }
    if (dz < 0.0f) {
        state->incrementZ = -state->incrementZ;
    }
    state->phaseX = 0.0f;
    state->phaseZ = 0.0f;
    state->startX = (float)state->currentX;
    state->startZ = (float)state->currentZ;
}
