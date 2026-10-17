extern int D_800ABAC8;
extern const double D_800AE6B0;

int func_0001c830(unsigned int frequency)
{
    unsigned int dacRate;
    unsigned char bitRate;
    float clock = (float)D_800ABAC8;
    double converted = (int)frequency;

    /* Use the original retained constant for the unsigned conversion. */
    if ((int)frequency < 0)
        converted += D_800AE6B0;
    dacRate = clock / (float)converted + 0.5f;
    if (dacRate < 132)
        return -1;
    bitRate = dacRate / 66;
    if (bitRate > 16)
        bitRate = 16;
    *(volatile unsigned int *)0xA4500010 = dacRate - 1;
    *(volatile unsigned int *)0xA4500014 = bitRate - 1;
    return D_800ABAC8 / (int)dacRate;
}
