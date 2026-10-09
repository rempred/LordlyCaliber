typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unk_00[0x12];
    u8 field_12;
    u8 unk_13[6];
    u8 related_class_19;
    u8 unk_1A[0x2E];
} ClassEntry;

u8 func_00043250(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C40;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_19 == alternate_class) {
        return classes[primary_class].field_12;
    }

    return classes[alternate_class].field_12;
}
