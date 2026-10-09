typedef unsigned char u8;

typedef struct ClassEntry {
    u8 unk_00[0x04];
    u8 unit_size_04;
    u8 unk_05[0x40];
    u8 related_class_45;
    u8 unk_46[0x02];
} ClassEntry;

u8 func_00043d1c(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C14;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_45 == alternate_class) {
        return classes[primary_class].unit_size_04;
    }

    return classes[alternate_class].unit_size_04;
}
