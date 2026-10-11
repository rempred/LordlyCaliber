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
unsigned char func_00043ad8(int sourceArt, int ownerContext);
typedef struct TerminalObject TerminalObject;
int func_00222190(TerminalObject *object, int x0, int z0, int x1, int z1);
int resource_free(unsigned char *resource);
void *resource_alloc(unsigned int bytes);
void memset_00023780(void *destination, int bytes);
void memcpy(void *source, void *destination, unsigned int bytes);
void func_0026C9CC(int count, float *first, float *second, float *output);
extern void *D_801CE8BC;
/* Bounded views into the existing assembly-owned data_00228d90 and
 * float_00228f94 bytes. The first view crosses their labels; none emits data. */
extern float D_801E5CC0[5];
extern float D_801E5CD4[5];
extern float D_801E5CE8[7];
extern float D_801E5D04[7];
extern float D_801E5D20[7];
extern float D_801E5D3C[7];
extern float D_801E5D58[3];
extern float D_801E5D64[3];

void func_0021D7F0(TerminalMotionState *state, int x, int z)
{
    float dx;
    float dz;
    float speed;
    float angle;
    float incrementX;
    float incrementZ;
    int duration;
    unsigned char slot;
    int count;
    int selector;
    int i;
    void *allocation;

    state->destinationX = (float)x;
    state->destinationZ = (float)z;
    dx = (float)(int)((unsigned int)x - state->currentX);
    dz = (float)(int)((unsigned int)z - state->currentZ);
    speed = func_00201670(state->actor->sourceArt, state->actor->ownerContext);
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

    if (func_00043ad8(state->actor->sourceArt & 0xFF,
                     state->actor->ownerContext & 0xFF) == 1) {
        duration = func_00222190((TerminalObject *)state->actor,
            state->currentX, state->currentZ, x, z);
        /* Combining this increment with the call adds a saved GPR in GCC. */
        duration++;
        slot = ((TerminalProfileContext *)D_801CE8BC)->nextProfile % 10;
        count = 0;
        if (((TerminalProfileContext *)D_801CE8BC)->profiles[slot] != 0) {
            resource_free((unsigned char *)((TerminalProfileContext *)D_801CE8BC)->profiles[slot]);
        }
        allocation = resource_alloc(0x84);
        ((TerminalProfileContext *)D_801CE8BC)->profiles[slot] = allocation;
        memset_00023780(allocation, 0x84);
        ((TerminalProfileContext *)D_801CE8BC)->nextProfile++;
        state->profile = ((TerminalProfileContext *)D_801CE8BC)->profiles[slot];

        selector = state->actor->ownerContext;
        /* Preserve the retail predicate groups with the pinned compiler. */
        if (selector == 0x4A) {
            count = 5;
            memcpy(D_801E5CC0, state->profile->first, 0x14);
            memcpy(D_801E5CD4, state->profile->second, 0x14);
        } else if (selector == 0x49) {
            count = 7;
            memcpy(D_801E5CE8, state->profile->first, 0x1C);
            memcpy(D_801E5D04, state->profile->second, 0x1C);
        } else if ((unsigned int)selector - 0x24u < 2u ||
                   (unsigned int)selector - 0x27u < 2u ||
                   ((selector == 0x29) | (selector == 0x45)) ||
                   ((selector == 0x46) | (selector == 0x79))) {
            count = 7;
            memcpy(D_801E5D20, state->profile->first, 0x1C);
            memcpy(D_801E5D3C, state->profile->second, 0x1C);
        } else if (selector == 0x47 || selector == 0x48) {
            count = 3;
            memcpy(D_801E5D58, state->profile->first, 0xC);
            memcpy(D_801E5D64, state->profile->second, 0xC);
            duration -= 15;
        }
        /* The index-based entry test retains the compiler's eight-byte home;
         * a guarded do-while performs the work with a shorter, nonretail frame. */
        for (i = 0; i < count; i++) {
            state->profile->first[i] *= (float)duration;
        }
        state->profile->index = 0;
        state->profile->count = count;
        state->profile->duration = duration;
        func_0026C9CC(state->profile->count, state->profile->first,
                     state->profile->second, state->profile->output);
    }
}

