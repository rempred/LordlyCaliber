typedef unsigned char u8;
typedef unsigned short u16;

typedef struct ClassEntry {
    u8 unk_00[0x14];
    u16 mental_base_14;
    u8 unk_16[0x2B];
    u8 related_class_41;
    u8 unk_42[0x06];
} ClassEntry;

u16 func_000435ec(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C18;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_41 == alternate_class) {
        return classes[primary_class].mental_base_14;
    }

    return classes[alternate_class].mental_base_14;
}
