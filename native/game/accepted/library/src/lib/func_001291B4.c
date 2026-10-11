typedef unsigned char u8;
typedef unsigned int u32;
typedef struct { u8 bytes[25]; } Record25;

extern Record25 g_func_001957D0_source_records[];
extern u8 func_00129068(u8 *record);
extern int func_00040f88(int, int, int, int, int);

u8 func_001291B4(u8 *record)
{
    u8 values[5];
    int i;
    unsigned int value;

    for (i = 0; i < 5; i++) {
        /* Preserve the retail base/index order in the address addition. */
        value = *(u8 *)((u32)record + i + 2);
        if (value == 0xFF)
            values[i] = 0;
        else
            values[i] = func_00129068(g_func_001957D0_source_records[value].bytes);
    }
    return func_00040f88(values[0], values[1], values[2], values[3], values[4]);
}
