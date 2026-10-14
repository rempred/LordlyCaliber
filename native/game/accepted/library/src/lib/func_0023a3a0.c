#include "common/types.h"
#include "game/class_entry.h"

typedef struct LinkRecord {
    u8 field_00[0x11];
    u8 class_11;
    u8 class_12;
    u8 field_13;
    u8 field_14;
    u8 field_15[0x15];
    u16 field_2A;
    u16 field_2C;
    u16 field_2E;
    u16 field_30;
    u16 field_32;
} LinkRecord;

extern ClassEntry D_80187C40[];
extern LinkRecord D_80195560[];
extern const void *func_8016E1DC(u8 first, u8 second);
extern void func_8009C550(void *destination, const void *source);
extern u16 func_8016DF88(u8 first, u8 second);
extern u16 func_8016DFDC(u8 first, u8 second);
extern u16 func_8016E030(u8 first, u8 second);
extern u16 func_8016E084(u8 first, u8 second);
extern u8 func_8016F5C8(u16 value);
extern void func_8016F11C(LinkRecord *record, u8 value);
extern void func_000FC120(s32 first, u8 index);
extern u8 func_8016DE70(u8 first, u8 second);
extern u32 func_8009C7CC(void);

void func_0023A3A0(u8 *record, u32 kind, s32 value,
                   u32 field_a, u32 field_b)
{
    u32 related = D_80187C40[kind].related_class_19;
    u8 category;
    u32 final_category;

    if (related == 0) {
        related = kind;
    }
    func_8009C550(record, func_8016E1DC(kind, related));
    ((LinkRecord *)record)->class_11 = kind;
    ((LinkRecord *)record)->class_12 = related;
    ((LinkRecord *)record)->field_13 = value;
    ((LinkRecord *)record)->field_2A = func_8016DF88(kind, related);
    ((LinkRecord *)record)->field_2C = func_8016DFDC(kind, related);
    ((LinkRecord *)record)->field_2E = func_8016E030(kind, related);
    ((LinkRecord *)record)->field_30 = func_8016E084(kind, related);

    if (field_a != 0) {
        category = func_8016F5C8(field_a);
        if (func_8016F5C8(((LinkRecord *)record)->field_2A) == category) {
            ((LinkRecord *)record)->field_2A = field_a;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_2C) == category) {
            ((LinkRecord *)record)->field_2C = field_a;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_2E) == category) {
            ((LinkRecord *)record)->field_2E = field_a;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_30) == category) {
            ((LinkRecord *)record)->field_30 = field_a;
        }
    }
    if (field_b != 0) {
        category = func_8016F5C8(field_b);
        if (func_8016F5C8(((LinkRecord *)record)->field_2A) == category) {
            ((LinkRecord *)record)->field_2A = field_b;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_2C) == category) {
            ((LinkRecord *)record)->field_2C = field_b;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_2E) == category) {
            ((LinkRecord *)record)->field_2E = field_b;
        }
        if (func_8016F5C8(((LinkRecord *)record)->field_30) == category) {
            ((LinkRecord *)record)->field_30 = field_b;
        }
    }
    func_8016F11C((LinkRecord *)record, value);
    func_000FC120(1, (LinkRecord *)record - D_80195560);
    final_category = func_8016DE70(kind, related);
    if (final_category == 3) {
        ((LinkRecord *)record)->field_14 = 0;
    } else if (final_category == 4) {
        ((LinkRecord *)record)->field_14 = func_8009C7CC() & 1;
    } else {
        ((LinkRecord *)record)->field_14 = final_category;
    }
}
