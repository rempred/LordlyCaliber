typedef struct TerminalObject {
    unsigned char prefix[0x48];
    int sourceArt;
    int ownerContext;
} TerminalObject;

float vec3_distance(const float first[3], const float second[3]);
float func_002016F4(int sourceArt, int ownerContext);

int func_00222344(TerminalObject *object, int x0, int z0, int x1, int z1)
{
    float first[3];
    float second[3];
    float distance;
    int allocationValue;

    /* The plain form loads the fifth integer directly through COP1 and is
     * eight bytes short. Reuse the equivalent word identity proven for the
     * corresponding incoming argument in func_00222190. */
    allocationValue = z1 | x1;
    z1 = allocationValue & z1;

    first[0] = (float)x1;
    first[1] = 0.0f;
    first[2] = (float)z1;
    second[0] = (float)x0;
    second[1] = 0.0f;
    second[2] = (float)z0;
    distance = vec3_distance(first, second);
    distance /= func_002016F4(object->sourceArt, object->ownerContext);
    return (int)distance;
}
