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

float func_0002d3e0(float z, float x);
double cos(double angle);
double sin(double angle);

void func_0021E2C4(TerminalMotionState *state, float x, float z, unsigned char speed)
{
    float dx;
    float dz;
    float angle;
    float incrementX;
    float incrementZ;

    state->destinationX = x - (float)(int)(state->actor->word5C - state->currentX);
    state->destinationZ = z - (float)(int)(state->actor->word64 - state->currentZ);
    dx = state->destinationX - (float)state->currentX;
    dz = state->destinationZ - (float)state->currentZ;
    incrementX = dx;
    if (dx != 0.0f) {
        angle = func_0002d3e0(dz, dx);
        /* Retail repeats each trig call and narrows the double conditional. */
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
