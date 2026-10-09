typedef unsigned char u8;
typedef unsigned int u32;
typedef struct { u8 bytes[52]; } Record52;
typedef struct { u8 bytes[56]; } Record56;
typedef struct { u8 bytes[72]; } Record72;

extern u8 D_8018F481;
extern Record52 g_func_0019554C_records_52[];
extern Record56 g_func_0019554C_records_56[];
extern Record72 g_func_0019554C_class_records[];
extern int func_00040f88(int, int, int, int, int);

u8 func_00129068(u8 *record)
{
    u8 values[5];
    int i;
    unsigned int value;

    if (record[0] == 0x1F && D_8018F481 != 8) {
        return 7;
    }
    switch (D_8018F481) {
    case 0x27:
    case 0x2D:
    case 0x2E:
    case 0x2F:
    case 0x3C:
        if (record[0] == 0x20) {
            return 7;
        }
        break;
    }
    for (i = 0; i < 5; i++) {
        /* This address form keeps the retail base/index order in addu. */
        value = *(u8 *)((u32)record + i + 2);
        if (value == 0) {
            values[i] = 0;
        } else if (value >= 100) {
            values[i] = 2;
        } else {
            if (record[0] >= 0x1F) {
                value = g_func_0019554C_records_52[value].bytes[0x12];
            } else {
                value = g_func_0019554C_records_56[value].bytes[0x12];
            }
            values[i] = g_func_0019554C_class_records[value].bytes[0x2C];
        }
    }
    return func_00040f88(values[0], values[1], values[2], values[3], values[4]);
}
