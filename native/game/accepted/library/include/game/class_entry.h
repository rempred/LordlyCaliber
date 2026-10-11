#ifndef GAME_CLASS_ENTRY_H
#define GAME_CLASS_ENTRY_H

typedef struct ClassEntry {
    u16 field_00;
    u16 field_02;
    u16 field_04;
    u16 field_06;
    u16 field_08;
    u8 unk_0A[0x0F];
    u8 related_class_19;
    u8 unk_1A[0x2E];
} ClassEntry;

#endif
