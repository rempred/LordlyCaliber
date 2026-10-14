#include "common/types.h"
#include "game/class_entry.h"

typedef struct LinkRecord56 {
    u8 field_00[0x11];
    u8 class_11;
    u8 class_12;
    u8 field_13;
    u8 field_14[0x16];
    u16 field_2A;
    u16 field_2C;
    u16 field_2E;
    u16 field_30;
    u8 field_32[2];
    u8 field_34;
    u8 field_35[3];
} LinkRecord56;

typedef struct SpecialLinkState {
    u8 field_00;
    u8 field_01;
    u16 field_02;
    u16 selected[0x78];
} SpecialLinkState;

typedef struct LinkValueEntry {
    u16 value;
    u8 field_02;
    u8 count;
} LinkValueEntry;

extern SpecialLinkState D_80190F80;
extern LinkRecord56 D_80193BC0[];
extern ClassEntry D_80187C40[];
extern u8 D_801936AC;
extern LinkValueEntry D_80196B00[];
extern void memset_00023780(void *destination, s32 size);
extern u8 func_8016C048(void);
extern u32 func_8016DF88(u8 first, u8 second);
extern u32 func_8016DFDC(u8 first, u8 second);
extern u32 func_8016E030(u8 first, u8 second);
extern u32 func_8016E084(u8 first, u8 second);
extern u16 func_8016B6FC(u16 value);
extern void func_8016F11C(LinkRecord56 *record, u8 value);
extern void func_801D4CC0(LinkRecord56 *record);
extern void func_000FC120(s32 first, u8 index);
extern void func_8016C0C8(u8 index);

/* Each site has the same native inline update, including the bounded free-entry
 * scan. Keep the default and selected values as separate actual updates. */
#define ADD_VALUE(value_) {                                                   \
    u32 entry_index = func_8016B6FC((u16)(value_));                            \
    if ((u16)entry_index < 0x116) {                                          \
        u32 count = D_80196B00[(u16)entry_index].count;                        \
        if (count < 99) {                                                    \
            D_80196B00[(u16)entry_index].count = count + 1;                    \
        }                                                                   \
    } else {                                                                \
        for (entry_index = 0; (u16)entry_index < 0x116; entry_index++) {       \
            if (D_80196B00[(u16)entry_index].value == 0) {                     \
                D_80196B00[(u16)entry_index].value = (value_);                 \
                D_80196B00[(u16)entry_index].count = 1;                        \
                break;                                                      \
            }                                                               \
        }                                                                   \
    }                                                                       \
}

#define APPLY_VALUE(getter_, field_) {                                       \
    value = getter_(record->class_11, record->class_12);                       \
    if ((u16)value != 0) {                                                   \
        ADD_VALUE(value);                                                   \
        if (record->field_ == (u16)value) {                                   \
            record->field_ = 0;                                              \
        }                                                                   \
    }                                                                       \
    {                                                                       \
        /* Test before assigning the persistent value, preserving the        \
         * compiler's scheduled copy in the lookup call's delay slot. */     \
        u32 selected_value = record->field_;                                 \
        if ((u16)selected_value != 0) {                                      \
            value = selected_value;                                         \
            ADD_VALUE(value);                                               \
        }                                                                   \
    }                                                                       \
}

/* Both final inputs are consumed as bytes. Their widths also preserve the
 * compiler's initial argument lifetimes and saved-register allocation. */
u32 func_00249290(u16 kind, u16 *selected, s8 modifier, u8 option)
{
    LinkRecord56 *record;
    u32 index;
    u32 value;

    if (kind == 1) {
        u8 remaining;
        for (index = 0; (u8)index < 0x78; index++) {
            if (D_80190F80.selected[(u8)index] == 0) {
                D_80190F80.selected[(u8)index] = D_80190F80.field_02;
                break;
            }
        }
        remaining = 200 - D_80190F80.field_00;
        if ((u8)remaining >= 3) {
            D_80190F80.field_00 += 3;
        } else {
            u32 previous = D_80190F80.field_01;
            D_80190F80.field_00 = 200;
            previous += 0xFD;
            D_80190F80.field_01 = remaining + previous;
        }
        return (u8)(index + 0x64);
    }

    for (index = 1; (u8)index < 100; index++) {
        if (D_80193BC0[(u8)index].class_11 == 0) {
            break;
        }
    }
    record = &D_80193BC0[(u8)index];
    memset_00023780(record, 0x38);
    record->field_34 = func_8016C048();
    record->class_11 = kind;
    record->class_12 = D_80187C40[kind].related_class_19;
    if (record->class_12 == 0) {
        record->class_12 = record->class_11;
    }
    record->field_13 = D_801936AC + modifier + 1;
    if (selected != 0) {
        record->field_2A = selected[0];
        record->field_2C = selected[1];
        record->field_2E = selected[2];
        record->field_30 = selected[3];
    }

    APPLY_VALUE(func_8016DF88, field_2A);
    APPLY_VALUE(func_8016DFDC, field_2C);
    APPLY_VALUE(func_8016E030, field_2E);
    APPLY_VALUE(func_8016E084, field_30);
    func_8016F11C(record, record->field_13);
    if ((u8)option == 1) {
        func_801D4CC0(record);
    }
    func_000FC120(0, (u8)index);
    func_8016C0C8((u8)index);
    return (u8)index;
}
