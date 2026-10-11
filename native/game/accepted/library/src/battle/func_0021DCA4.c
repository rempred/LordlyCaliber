typedef struct TerminalMotionActor {
    unsigned char prefix[0x48];
    int sourceArt;
    int ownerContext;
    unsigned char gap50[0xC];
    unsigned int word5C;
    unsigned int word60;
    unsigned int word64;
} TerminalMotionActor;

/* Each observed profile has at most seven entries in the 0x84 allocation. */
typedef struct TerminalProfile {
    int index;
    int count;
    int duration;
    float first[7];
    unsigned char gap28[0xC];
    float second[7];
    unsigned char gap50[0xC];
    float output[7];
    unsigned char gap78[0xC];
} TerminalProfile;

typedef struct TerminalProfileContext {
    unsigned char prefix[0x180];
    TerminalProfile *profiles[10];
    unsigned char nextProfile;
} TerminalProfileContext;

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
    int wordB8;
    TerminalProfile *profile;
} TerminalMotionState;

float func_00201670(int sourceArt, int ownerContext);
float func_0002d3e0(float z, float x);
double cos(double angle);
double sin(double angle);
typedef struct TerminalObject TerminalObject;
int func_0022222C(TerminalObject *source, TerminalObject *target, int x, int z);
int resource_free(unsigned char *resource);
void *resource_alloc(unsigned int bytes);
void memset_00023780(void *destination, int bytes);
void memcpy(void *source, void *destination, unsigned int bytes);
void func_0026C9CC(int count, float *first, float *second, float *output);
extern void *D_801CE8BC;
typedef struct TerminalTriple { float values[3]; } TerminalTriple;
extern TerminalTriple D_801E6C30;
extern TerminalTriple D_801E6C3C;

void func_0021DCA4(TerminalMotionState *state, TerminalMotionActor *target)
{
    float dx;
    float dz;
    float speed;
    float angle;
    float incrementX;
    float incrementZ;
    int duration;
    int queryResult;
    unsigned char slot;


    unsigned int i;
    void *allocation;

    state->destinationX = (float)(int)target->word5C;
    state->destinationZ = (float)(int)target->word64;
    dx = (float)(int)(target->word5C - state->currentX);
    dz = (float)(int)(target->word64 - state->currentZ);
    speed = func_00201670(state->actor->sourceArt, state->actor->ownerContext);
    speed += speed;
    incrementX = dx;
    if (dx != 0.0f) {
        angle = func_0002d3e0(dz, dx);
        /* Retain repeated retail calls and narrow the double conditional. */
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

    queryResult = func_0022222C((TerminalObject *)state->actor,
        (TerminalObject *)target, state->currentX, state->currentZ);
    {
        TerminalTriple first = D_801E6C30;
        TerminalTriple second = D_801E6C3C;

        duration = queryResult / 2;
        duration++;
        slot = ((TerminalProfileContext *)D_801CE8BC)->nextProfile % 10;
        if (((TerminalProfileContext *)D_801CE8BC)->profiles[slot] != 0) {
            resource_free((unsigned char *)((TerminalProfileContext *)D_801CE8BC)->profiles[slot]);
        }
        allocation = resource_alloc(0x84);
        ((TerminalProfileContext *)D_801CE8BC)->profiles[slot] = allocation;
        memset_00023780(allocation, 0x84);
        ((TerminalProfileContext *)D_801CE8BC)->nextProfile++;
        state->profile = ((TerminalProfileContext *)D_801CE8BC)->profiles[slot];
        memcpy(first.values, state->profile->first, 0xC);
        memcpy(second.values, state->profile->second, 0xC);
        for (i = 0; i < 3u; i++) {
            state->profile->first[i] *= (float)duration;
        }
        state->profile->index = 0;
        state->profile->count = 3;
        state->profile->duration = duration;
        func_0026C9CC(state->profile->count, state->profile->first,
                     state->profile->second, state->profile->output);
    }
}
