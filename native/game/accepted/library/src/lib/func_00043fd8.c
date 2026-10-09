typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unknown_00[0x0B];
    u8 field_0B;
    u8 unknown_0C[0x0D];
    u8 related_class_19;
    u8 unknown_1A[0x2E];
} ClassEntry;

int func_00044340(int class_id);

int func_00043fd8(int primary_class, int alternate_class)
{
    ClassEntry *classes = (ClassEntry *)0x80187C40;
    u8 value;

    primary_class &= 0xFF;
    alternate_class &= 0xFF;
    if (classes[primary_class].related_class_19 == alternate_class) {
        value = classes[primary_class].field_0B;
    } else {
        value = classes[alternate_class].field_0B;
    }

    return func_00044340(value);
}
