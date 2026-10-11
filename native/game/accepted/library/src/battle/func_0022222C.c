typedef struct TerminalObject {
    unsigned char prefix[0x48];
    int sourceArt;
    int ownerContext;
    int word50;
    int word54;
    int word58;
} TerminalObject;

int func_0020BF50(int value);
int func_0020BF34(int value);
unsigned char func_00201838(int sourceArt, int ownerContext);
unsigned char func_002018A0(int sourceArt, int ownerContext);
float vec3_distance(const float first[3], const float second[3]);
float func_00201670(int sourceArt, int ownerContext);

int func_0022222C(TerminalObject *source, TerminalObject *target, int x, int z)
{
    float first[3];
    float second[3];
    int mappedX;
    int mappedZ;
    unsigned char sourceAdjustment;
    unsigned char targetAdjustment;
    int adjustment;
    float distance;

    mappedX = func_0020BF50(target->word58);
    mappedZ = func_0020BF34(target->word54);
    sourceAdjustment = func_00201838(source->sourceArt, source->ownerContext);
    targetAdjustment = func_002018A0(target->sourceArt, target->ownerContext);
    adjustment = (int)sourceAdjustment + (int)targetAdjustment;
    first[0] = (float)x;
    first[1] = 0.0f;
    first[2] = (float)z;
    second[0] = (float)mappedX;
    second[1] = 0.0f;
    second[2] = (float)mappedZ;
    distance = vec3_distance(first, second);
    distance -= (float)adjustment;
    distance /= func_00201670(source->sourceArt, source->ownerContext);
    return (int)distance;
}
