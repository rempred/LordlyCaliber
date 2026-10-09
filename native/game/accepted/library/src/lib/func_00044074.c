typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unknown_00[0x0F];
    u8 field_0F;
    u8 unknown_10[9];
    u8 related_class_19;
    u8 unknown_1A[0x2E];
} ClassEntry;

int func_00044340(int class_id);

int func_00044074(int primary_class, int alternate_class)
{
    ClassEntry *classes = (ClassEntry *)0x80187C40;
    u8 value;

    primary_class &= 0xFF;
    alternate_class &= 0xFF;
    if (classes[primary_class].related_class_19 == alternate_class) {
        value = classes[primary_class].field_0F;
    } else {
        value = classes[alternate_class].field_0F;
    }

    return func_00044340(value);
}
