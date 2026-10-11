typedef unsigned char u8;
typedef unsigned short u16;

#include "game/class_entry.h"

u16 func_00043e88(int arg0, int arg1)
{
    ClassEntry *classes;
    int primary_class;
    int alternate_class;

    classes = (ClassEntry *)0x80187C40;
    primary_class = arg0 & 0xFF;
    alternate_class = arg1 & 0xFF;
    if (classes[primary_class].related_class_19 == alternate_class) {
        return classes[primary_class].field_02;
    }

    return classes[alternate_class].field_02;
}
