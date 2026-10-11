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

typedef struct TerminalUpdateState {
    int word00;
    int scaledX;
    int scaledY;
    int scaledZ;
    unsigned char gap10[0xC];
    short currentX;
    short currentY;
    short currentZ;
    unsigned char gap22[0x76];
    float destinationX;
    float destinationZ;
    unsigned char gapA0[0x1C];
    TerminalProfile *profile;
} TerminalUpdateState;

typedef struct TerminalUpdateContext {
    TerminalUpdateState *first[3];
    TerminalUpdateState *second[3];
    unsigned char gap18[0x30];
    int sourceArt;
    int ownerContext;
    unsigned char gap50[0x55];
    unsigned char activeMask;
} TerminalUpdateContext;

typedef struct TerminalPosition TerminalPosition;
unsigned char func_00043ad8(int sourceArt, int ownerContext);
void func_001F10F0(void *state, void *destination);
int func_00222530(TerminalPosition *first, TerminalPosition *second);
float func_001FF7EC(short x, short z);
float func_0026CBDC(float position, int count, float *first, float *second, float *output);

int func_002213DC(TerminalUpdateContext *context)
{
    unsigned int i;
    TerminalUpdateState *first;
    TerminalUpdateState *second;
    int height;
    int oldIndex;
    int narrowedHeight;
    float increment;

    for (i = 0; i < 3u; i++) {
        if (((context->activeMask >> i) & 1) != 0) {
            first = context->first[i];
            second = context->second[i];
            if ((first != 0) && (second != 0)) {
                if (func_00043ad8(context->sourceArt & 0xFF,
                                 context->ownerContext & 0xFF) != 1) {
                    func_001F10F0(first, &first->destinationX);
                    func_00222530((TerminalPosition *)first, (TerminalPosition *)second);
                    first->scaledX = first->currentX * 30;
                    first->scaledY = first->currentY * 30;
                    first->scaledZ = first->currentZ * 30;
                    second->currentX = first->currentX;
                    second->currentZ = first->currentZ;
                } else {
                    if (first->profile->index >= first->profile->duration) {
                        return 1;
                    }
                    func_001F10F0(first, &first->destinationX);
                    height = (int)func_001FF7EC(first->currentX, first->currentZ);
                    first->currentY = height;
                    second->currentY = height;
                    second->currentX = first->currentX;
                    second->currentZ = first->currentZ;
                    oldIndex = first->profile->index;
                    first->profile->index = (unsigned int)oldIndex + 1u;
                    increment = func_0026CBDC((float)oldIndex,
                        first->profile->count, first->profile->first,
                        first->profile->second, first->profile->output);
                    first->currentY = (int)((float)first->currentY + increment);
                    first->scaledX = first->currentX * 30;
                    narrowedHeight = first->currentY;
                    first->scaledZ = first->currentZ * 30;
                    first->scaledY = narrowedHeight * 30;
                }
            }
        }
    }
    return 0;
}
