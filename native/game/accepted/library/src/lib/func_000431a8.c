typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unk_00[0x0A];
    u8 field_0A;
    u8 unk_0B[0x0E];
    u8 related_class_19;
    u8 unk_1A[0x2E];
} ClassEntry;

u8 func_000431a8(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C40;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_19 == alternate_class) {
        return classes[primary_class].field_0A;
    }

    return classes[alternate_class].field_0A;
}
