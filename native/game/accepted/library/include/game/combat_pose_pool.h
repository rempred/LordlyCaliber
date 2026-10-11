#ifndef COMBAT_POSE_POOL_H
#define COMBAT_POSE_POOL_H

#include "combat_types.h"
typedef unsigned char u8;

/* The pool routines traverse records at a 0xB0-byte stride. Field names
 * describe offsets; allocation and traversal do not establish occupancy.
 */
typedef struct CombatPosePoolRecord {
    int field_00;
    u32 field_04;
    u32 *field_08;
    int field_0C;
    u32 field_10;
    u32 field_14;
    u32 field_18;
    u32 field_1C;
    u32 field_20;
    u32 field_24;
    u32 field_28;
    u8 *field_2C;
    u8 *field_30;
    u32 field_34;
    void **field_38;
    void **field_3C;
    void **field_40;
    u32 *field_44;
    u32 *field_48;
    u32 field_4C;
    u32 field_50;
    u8 *field_54;
    u8 *field_58;
    u8 *field_5C;
    u8 *field_60;
    u8 *field_64;
    u8 *field_68;
    void *field_6C;
    void *field_70[10];
    u32 field_98;
    u32 field_9C;
    u16 field_A0;
    u8 field_A2[6];
    u16 field_A8;
    u16 field_AA;
    u16 field_AC;
    u16 field_AE;
} CombatPosePoolRecord;

extern unsigned char D_801CEF00;
extern CombatPosePoolRecord *D_801D0728;
extern void *func_00001330(u32 bytes);
extern void func_000016C4(void *allocation);
extern void func_00023780(void *destination, u32 bytes);

#endif
