int func_000458f0(int itemId)
{
    int type = ((unsigned char *)0x8018C410)[(itemId & 0xFFFF) * 32];

    /* Separate returns retain KMC's branch chain; a combined boolean is shorter. */
    if (type != 0) {
        if (type < 14) {
            return 1;
        }
        if (type == 24) {
            return 1;
        }
    }
    return 0;
}
