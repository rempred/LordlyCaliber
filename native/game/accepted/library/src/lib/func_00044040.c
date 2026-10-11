typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unknown_00[0x0D];
    u8 field_0D;
    u8 unknown_0E[0x3A];
} ClassEntry;

int func_00044340(int class_id);

int func_00044040(int unused, int class_id)
{
    ClassEntry *classes = (ClassEntry *)0x80187C40;
    u8 value;

    do {
        value = classes[class_id & 0xFF].field_0D;
    } while (0);
    return func_00044340(value);
}
