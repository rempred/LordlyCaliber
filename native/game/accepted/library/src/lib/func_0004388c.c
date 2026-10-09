typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unk_00[0x21];
    u8 physical_resistance_21;
    u8 unk_22[0x1F];
    u8 related_class_41;
    u8 unk_42[0x06];
} ClassEntry;

u8 func_0004388c(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C18;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_41 == alternate_class) {
        return classes[primary_class].physical_resistance_21;
    }

    return classes[alternate_class].physical_resistance_21;
}
