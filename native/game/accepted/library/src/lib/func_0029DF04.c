typedef signed short s16;
typedef unsigned char u8;

extern char *D_8022A974;

int func_80233968(int bank, int state, int cursor, int variant,
                  u8 *value0, u8 *value1, u8 *value2);

/* The dispatcher consumes a word result. Narrow here, where retail does so. */
int func_0029DF04(int slot)
{
    char *actor = *(char **)(D_8022A974 + slot * 4 + 0x18);
    u8 value0;
    u8 value1;
    u8 value2;

    return (u8)func_80233968(*(int *)(actor + 0xE8),
                         *(s16 *)(actor + 0x134),
                         *(int *)(actor + 0xF0),
                         *(u8 *)(actor + 0x146),
                         &value0, &value1, &value2);
}
