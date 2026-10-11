typedef struct TerminalObject {
    unsigned char prefix[0x48];
    int sourceArt;
    int ownerContext;
} TerminalObject;

float vec3_distance(const float first[3], const float second[3]);
float func_00201670(int sourceArt, int ownerContext);

int func_00222190(TerminalObject *object, int x0, int z0, int x1, int z1)
{
    float first[3];
    float second[3];
    float distance;
    int allocationValue;

    /* Pinned GCC needs this equivalent word identity to retain the incoming
     * integer in a general register. The plain form uses a direct COP1 load
     * and is eight bytes shorter than retail. */
    allocationValue = z1 | x1;
    z1 = allocationValue & z1;

    first[0] = (float)x0;
    first[1] = 0.0f;
    first[2] = (float)z0;
    second[0] = (float)x1;
    second[1] = 0.0f;
    second[2] = (float)z1;
    distance = vec3_distance(first, second);
    distance /= func_00201670(object->sourceArt, object->ownerContext);
    return (int)distance;
}
