#include "game/scenario_source_record.h"

typedef double f64;

/* The local work word follows the original cursor, category-value, next-cursor phases. */
typedef union ShopInventoryWorkValue {
    u16 *list;
    u32 category_value;
} ShopInventoryWorkValue;
typedef char ShopInventoryWorkValueSizeCheck[sizeof(ShopInventoryWorkValue) == 4 ? 1 : -1];

/* The retail row initializer writes five halfwords, then advances ten bytes. */
typedef struct ShopCategoryRow {
    s16 first_index;
    u16 field_02;
    u16 field_04;
    s16 last_index;
    u16 field_08;
} ShopCategoryRow;

typedef struct ShopInventoryScratch {
    u32 equipment[50];
    u32 consumables[15];
    u8 equipment_counts[6];
    u8 ordered_categories[7];
    u8 category_cursor;
    u8 last_category;
} ShopInventoryScratch;

/* These fields are identified by offsets; their UI meanings remain tentative. */
typedef struct ShopUiState {
    u8 tick;
    u8 field_01;
    u8 field_02;
    u8 field_03;
    u8 motion_flags;
    u8 prompt_code;
    u8 prompt_variant;
    u8 field_07;
    u32 selected_id;
    u32 price;
    void *arg_10;
    void *arg_14;
    u8 payload_18[8];
    u16 *combined_list;
    u32 field_24;
    void *object_28;
    void *object_2C;
    void *object_30;
    void *object_34;
    void *object_38;
    void *object_3C;
    ShopCategoryRow *category_rows;
} ShopUiState;

typedef struct ShopWidget {
    u8 gap_00[0x32];
    s16 field_32;
    u8 gap_34[0x9C];
    void *field_D0;
} ShopWidget;

typedef struct ShopValueRecord12 {
    u16 field_00;
    u8 gap_02[10];
} ShopValueRecord12;
typedef struct ShopValueRecord32 {
    u16 field_00;
    u8 gap_02[30];
} ShopValueRecord32;
typedef struct ShopValueRecord56 {
    u8 bytes[56];
} ShopValueRecord56;

extern u8 D_80219D24;
extern ShopUiState D_80219F00;
extern ShopInventoryScratch D_8021A010;
extern u8 D_8021A123, D_8021A124;
extern u8 D_8021A114[6], D_8021A11A[7], D_8021A121, D_8021A122;
extern u32 D_8021A0D8[15];

/* D_80219F0C and D_80219F00.price address the same price word. */
extern volatile u8 D_80219F02;
extern void *D_80219F38;
extern u32 D_80219F0C;
extern u16 *D_80219F20;
extern u8 D_80219F03;
/* Separate view of the observed motion byte at state +4. */
extern u8 D_80219F04;
extern u32 D_80219F08;
extern ShopWidget *D_80219F30;

extern u32 D_80196A6C;
extern u8 D_8019EE40;
extern s32 D_801F3658;
extern u8 D_801F365B;
extern u16 *D_801F0CA0;
extern void *D_801F0CB0[];
extern u16 *D_800C4BDC;
extern u16 *D_800C4C4C;
extern u8 D_80219C28[];
extern void *D_80219C5C[];
extern ShopValueRecord12 D_8018E6D2[];
extern ShopValueRecord32 D_8018C414[];
extern u8 D_801971F2[][25];
extern u8 D_801971FD[][25];
extern ShopValueRecord56 D_80193BC0[];
extern f64 D_80219DA0, D_80219DA8;
extern u8 D_800EB1F0;
extern u8 D_80216164, D_80216C54, D_8021794C, D_80217A34;
extern u8 D_802182E4, D_802181A4;

extern void memset_00023780(void *destination, s32 size);
extern void *resource_alloc(s32 size);
extern void resource_free(void *resource);
extern u8 func_0019B1B0(u32 equipment_id);
extern void func_0019B800(s32 count);
extern void func_0019BB34(void);
extern void *func_00051eb0(s32, s32, const void *, s32, s32, s32,
                           s32, s32, s32, s32, s32);
extern void func_0019ADF0(void *object, s32 item, s32 mode);
extern s32 func_801DCF90(void *unit);
extern s32 func_0005148c(void *record);
extern void func_0019BAB4(void);
extern void func_0015DF10(s32 value);
extern u8 func_802189BC(u8 category);
extern u32 func_0019BD14(void);
extern u8 func_0019B26C(u16 selected, u8 mode);
extern s32 func_000453e0(u16 value);
extern s32 func_000454b0(u16 value);
extern void func_0019BAE4(u8 quantity, void *payload);
extern void func_0019B710(u8 *quantity, u8 value);
extern void func_0019B4C4(u16 selected, u8 quantity, s32 consumable);
extern u8 func_001349BC(u8, void *, void *, s32, s32, s32, s32);
extern u32 func_0014F300(s32 source_word);
extern void func_0003fb94(const void *, s32);
extern s32 func_00134830(u8, void *, s32, s32, u8, s32);
extern void func_0019B63C(u16 selected, u8 count);
extern void func_00052284(void *object);
extern void func_0019BCA0(void);
extern void func_0005219c(void *, s32, s16, s32, s32);
extern void func_0015EF40(ShopCategoryRow *row);

u8 func_001977E0(void)
{
    u8 state = D_80219D24;
    u8 tick_next;

    switch (state) {
    case 0: {
        u16 *original_list = D_80219F20;
        ShopInventoryWorkValue current;
        s32 equipment_count;
        s32 consumable_count;
        s32 index;
        u32 *equipment_base = D_8021A010.equipment;
        u32 *consumable_base = D_8021A0D8;
        u32 *equipment_cursor;
        u32 *consumable_cursor;
        ShopCategoryRow *row;
        u8 *ordered_cursor;

        memset_00023780(equipment_base, 0xC8);
        memset_00023780(consumable_base, 0x3C);
        memset_00023780(D_8021A114, 6);
        current.list = D_80219F20;
        consumable_count = 0;
        equipment_count = 0;

        if (*current.list != 0) {
            consumable_cursor = consumable_base;
            equipment_cursor = equipment_base;
            do {
                u16 entry = *current.list;
                if (entry & 0x8000) {
                    s32 equipment_id = entry & 0x7FFF;
                    u8 category_id;
                    u16 helper_input;
                    u8 category_count;
                    *equipment_cursor++ = equipment_id;
                    helper_input = *current.list;
                    category_id = func_0019B1B0(helper_input & 0x7FFF);
                    category_count = D_8021A114[category_id];
                    /* All old list reads finish before the category-value phase. */
                    current.category_value = category_count + 1;
                    D_8021A114[category_id] = current.category_value;
                    equipment_count++;
                } else {
                    *consumable_cursor++ = entry;
                    consumable_count++;
                }
                current.list = D_80219F20 + 1;
                D_80219F20 = current.list;
            } while (*current.list != 0);
        }
        resource_free(original_list);
        func_0019B800(equipment_count);
        index = 0;
        equipment_count = 0;

        /* Retail keeps a count and an advancing ordered output. */
        ordered_cursor = D_8021A11A;
        for (; index < 6; index++) {
            if (D_8021A114[index] != 0) {
                *ordered_cursor++ = index;
                equipment_count++;
            }
        }
        if (D_8021A0D8[0] != 0) {
            D_8021A11A[equipment_count++] = 6;
        }
        D_8021A122 = equipment_count - 1;
        D_8021A121 = 0;
        func_0019BB34();

        D_80219F00.category_rows =
            (ShopCategoryRow *)resource_alloc(
                (D_8021A122 + 1) * 10);
        row = D_80219F00.category_rows;
        for (index = 0; index <= D_8021A122;
             index++) {
            ShopCategoryRow *active_row = &row[index];
            if (index == D_8021A122) {
                active_row->last_index = consumable_count - 1;
            } else {
                s8 category_id = (s8)D_8021A11A[index];
                active_row->last_index = D_8021A114[category_id] - 1;
            }
            active_row->field_08 = 0xFF;
            active_row->field_02 = 0;
            active_row->first_index = 0;
            active_row->field_04 = 4;
        }

        D_80219F00.object_28 = func_00051eb0(
            0x400, 0, &D_80216164, 0x18, 0x16, 0x20, 0x1E,
            0x18, 0x16, 0xB2, 0x6F);
        D_80219F00.object_2C = func_00051eb0(
            0, 0, &D_80216C54, 0x120, 0x18, 0x128, 0x20,
            0xBC, 0x18, 0x128, 0xAE);
        D_80219F00.object_30 = func_00051eb0(
            0x100, 0, &D_8021794C, 0x9C, 0xD6, 0xA4, 0xDE,
            0x18, 0xB6, 0x128, 0xDE);
        func_0019ADF0(D_80219F00.object_30,
                        D_8021A010.equipment[0], 0);
        D_80219F00.object_34 = func_00051eb0(
            0, 0, &D_80217A34, 0x18, 0x77, 0x20, 0x7F,
            0x18, 0x77, 0xB2, 0xAD);
        D_80219F00.object_38 = 0;
        D_80219F00.object_3C = 0;
        D_8021A123 = 1;
        D_80219F00.motion_flags = 0;
        D_80219F00.tick = 0;
        D_80219D24 = 1;
        break;
    }

    case 1: {
        if (D_80219F00.tick >= 9) {
            /* Byte-table values remain word-sized loop locals. */
            s32 selector = func_801DCF90(D_801F0CB0[D_801F3658]);
            s32 start = D_80219C28[selector * 2];
            s32 count = D_80219C28[selector * 2 + 1];
            s32 maximum = 0;
            s32 index;
            s32 half_width;
            for (index = 0; index < count; index++) {
                s32 current = func_0005148c(D_80219C5C[start + index]);
                s32 next_maximum;
                if (maximum < current) {
                    next_maximum = func_0005148c(D_80219C5C[start + index]);
                } else {
                    next_maximum = maximum;
                }
                maximum = next_maximum;
            }
            half_width = (maximum + 0x37) / 2;
            D_80219F00.object_38 = func_00051eb0(
                0x100, 0, &D_802182E4, 0x9C, 0x74, 0xA4, 0x7C,
                (s16)(0xA0 - half_width), 0x5A,
                (s16)(0xA0 + half_width), 0x96);
            ((u8 *)D_80219F00.object_38)[0x22] = selector;
            D_80219F00.tick = 0;
            D_80219D24 = 2;
        }
        goto tick_increment;
    }

    case 2: {
        u8 tick = D_80219F00.tick;
        if (tick < 9) {
            tick_next = tick + 1;
            goto tick_store;
        } else if ((*D_800C4BDC &
                    (D_801F0CA0[0] | D_801F0CA0[1])) != 0) {
            func_0019BAB4();
            func_0015DF10(6);
            D_80219D24 = 3;
        }
        break;
    }

    case 3: {
        u16 *input_words = D_801F0CA0;
        u16 input_mask = *D_800C4BDC;
        u8 category = D_8021A121;
        u32 price_to_check;
        if (input_mask & input_words[0]) {
            s32 selected;
            s32 price;
            /* Each item branch reads its signed row index. */
            if (category == D_8021A122) {
                s32 row_index = D_80219F00.category_rows[category].first_index;
                selected = D_8021A0D8[row_index];
                price = D_8018E6D2[selected].field_00;
                D_80219F00.selected_id = selected;
                D_80219F00.price = price;
                D_80219F00.field_02 = func_0019B26C(selected, 0);
            } else {
                u8 offset = func_802189BC(category);
                s32 row_index = D_80219F00.category_rows[D_8021A121].first_index;
                selected = D_8021A010.equipment[row_index + offset];
                /* Publish the equipment ID before the special-price call. */
                D_80219F00.selected_id = selected;
                if (selected == 0xFA) {
                    D_80219F0C = func_0019BD14();
                } else {
                    D_80219F0C = D_8018C414[selected].field_00;
                }
                D_80219F00.field_02 = func_0019B26C(D_80219F00.selected_id, 1);
            }
            price_to_check = D_80219F00.price;
            if (D_80196A6C < price_to_check) {
                D_80219F00.prompt_code = 0x4D;
                D_80219F00.prompt_variant = 1;
                D_80219F00.arg_10 = 0;
                D_80219D24 = 10;
                func_0015DF10(9);
            } else if (D_80219F02 == 0) {
                D_80219F00.prompt_code = 0x4E;
                D_80219F00.prompt_variant = 1;
                D_80219F00.arg_10 = 0;
                D_80219D24 = 10;
                func_0015DF10(9);
            } else {
                D_80219D24 = 4;
                func_0015DF10(6);
            }
        } else if (input_mask & input_words[1]) {
            D_80219D24 = 14;
            func_0015DF10(1);
        } else {
            u16 navigation = *D_800C4C4C;
            if (navigation & 0x200) {
                u8 next;
                if (category == 0) {
                    next = D_8021A122;
                } else {
                    next = category - 1;
                }
                D_8021A121 = next;
            } else if (navigation & 0x100) {
                D_8021A121 =
                    category < D_8021A122 ? category + 1 : 0;
            } else {
                func_0015EF40(&D_80219F00.category_rows[category]);
            }
            if ((*D_800C4C4C & 0xC0F) ||
                D_8021A121 != category) {
                /* Retail re-reads the category cursor along redraw. */
                s32 item;
                u8 displayed_category;
                s32 row_offset;
                s32 scaled_four;
                if (D_8021A121 != category) {
                    func_0015DF10(2);
                }
                resource_free(((ShopWidget *)D_80219F00.object_30)->field_D0);
                displayed_category = D_8021A121;
                /* Split the ten-byte row scale around the category test to
                 * retain retail's index shift in the branch delay slot. */
                scaled_four = displayed_category << 2;
                if (displayed_category == D_8021A122) {
                    ShopCategoryRow *selected_row;
                    row_offset = (scaled_four + displayed_category) << 1;
                    /* Form the observed byte offset before the 32-bit base. */
                    selected_row = (ShopCategoryRow *)(row_offset + (u32)D_80219F00.category_rows);
                    item = D_8021A0D8[selected_row->first_index];
                    func_0019ADF0(D_80219F00.object_30, item, 3);
                } else {
                    u8 offset = func_802189BC(D_8021A121);
                    item = D_8021A010.equipment[
                        D_80219F00.category_rows[D_8021A121].first_index +
                        offset];
                    func_0019ADF0(D_80219F00.object_30, item, 0);
                }
            }
        }
        break;
    }

    case 4: {
        u16 *input_words = D_801F0CA0;
        u16 mask = *D_800C4BDC;
        if (mask & input_words[0]) {
            if (D_8021A121 == D_8021A122) {
                D_80219F00.arg_10 =
                    (void *)func_000453e0((u16)D_80219F00.selected_id);
            } else {
                D_80219F00.arg_10 =
                    (void *)func_000454b0((u16)D_80219F00.selected_id);
            }
            func_0019BAE4(D_8021A123 & 0x7F,
                            D_80219F00.payload_18);
            D_80219F00.prompt_code = 0x56;
            D_80219F00.arg_14 = D_80219F00.payload_18;
            D_80219D24 = 7;
            func_0015DF10(6);
        } else if (mask & input_words[1]) {
            D_80219D24 = 3;
            D_8021A123 = 1;
            func_0015DF10(1);
        } else {
            func_0019B710(&D_8021A123,
                            D_80219F00.field_02);
        }
        break;
    }

    case 5: {
        u8 quantity = D_8021A123 & 0x7F;
        u8 remaining;
        if (quantity >= 11) {
            u32 *funds = &D_80196A6C;
            *funds -= D_80219F00.price * 10;
            remaining = quantity - 10;
        } else {
            u32 *funds = &D_80196A6C;
            *funds -= D_80219F00.price;
            remaining = quantity - 1;
        }
        func_0015DF10(0x12);
        if (remaining == 0) {
            func_0019B4C4((u16)D_80219F00.selected_id,
                            D_80219F00.field_01,
                            D_8021A121 !=
                            D_8021A122);
            D_8021A123 = 1;
            D_80219D24 = 3;
        } else {
            D_8021A123 =
                (D_8021A123 & 0x80) + remaining;
        }
        break;
    }

    case 6: {
        if (D_80219F00.tick >= 9) {
            u8 selector = D_801971F2[D_801F3658][0];
            D_80219F00.arg_10 = &D_80193BC0[selector];
            D_80219F00.arg_14 =
                (void *)func_000453e0((u16)D_80219F00.selected_id);
            D_80219F00.prompt_code = 0x55;
            D_80219D24 = 8;
        }
        goto tick_increment;
    }

    case 7:
    case 8: {
        u8 result = func_001349BC(D_80219F00.prompt_code,
                                  D_80219F00.arg_10,
                                  D_80219F00.arg_14, 0, 0, 2, 0x64);
        if (result == 1) {
            if (D_80219D24 == 7) {
                if (D_8019EE40 == 0) {
                    D_80219F00.field_01 =
                        D_8021A123 & 0x7F;
                    if (D_8021A121 ==
                        D_8021A122) {
                        s32 count = func_0014F300(D_801F365B);
                        s32 index = 0;
                        /* Keep the numeric bound and moving slot cursor. */
                        u8 *slot;
                        D_80219F03 = 0;
                        if (index < (u8)count) {
                            count = (u8)count;
                            slot = D_801971FD[D_801F3658];
                            do {
                                if (*slot == 0) {
                                    D_80219F03++;
                                }
                                index++;
                                slot++;
                            } while (index < count);
                        }
                        if (D_80219F03 != 0) {
                            D_80219F00.motion_flags = 2;
                            D_80219F00.tick = 0;
                            D_80219D24 = 6;
                            func_0015DF10(6);
                        } else {
                            D_80219F00.prompt_code = 0x4F;
                            D_80219F00.prompt_variant = 2;
                            D_80219F00.arg_10 = 0;
                            D_80219D24 = 11;
                            func_0003fb94(&D_800EB1F0, 0x2C7);
                        }
                    } else {
                        D_80219D24 = 5;
                        func_0003fb94(&D_800EB1F0, 0x2C7);
                    }
                } else {
                    D_80219D24 = 4;
                    func_0015DF10(1);
                }
            } else if (D_8019EE40 == 0) {
                u8 count;
                u8 quantity;
                D_80219F00.object_38 = func_00051eb0(
                    0, 0, &D_802181A4, 0x9C, 0x74, 0xA4, 0x7C,
                    0x64, 0x6C, 0xDC, 0x84);
                count = D_80219F03;
                quantity = D_8021A123;
                /* Read the new object pointer in each quantity branch. */
                if (quantity < count) {
                    ((u8 *)D_80219F38)[0x22] = quantity;
                } else {
                    ((u8 *)D_80219F38)[0x22] = count;
                }
                D_8021A124 = 1;
                D_80219D24 = 9;
                func_0015DF10(6);
            } else {
                D_80219F00.motion_flags = 1;
                D_80219F00.tick = 0;
                D_80219D24 = 13;
                func_0003fb94(&D_800EB1F0, 0x2C7);
            }
        } else if (result == 3) {
            D_80219F00.motion_flags = 1;
            D_80219D24 = 4;
            func_0015DF10(1);
        }
        break;
    }

    case 9: {
        u16 *input_words = D_801F0CA0;
        u16 mask = *D_800C4BDC;
        if (mask & input_words[0]) {
            func_0019BAB4();
            D_80219F00.arg_10 =
                (void *)func_000453e0((u16)D_80219F00.selected_id);
            D_80219F00.prompt_code = 0x58;
            D_80219F00.prompt_variant = 2;
            D_80219D24 = 12;
            func_0003fb94(&D_800EB1F0, 0x2C7);
        } else if (mask & input_words[1]) {
            func_0019BAB4();
            D_80219F00.tick = 0;
            D_80219D24 = 6;
            func_0015DF10(1);
        } else {
            u8 maximum = D_80219F00.field_03;
            u8 quantity = D_8021A123;
            u8 amount;
            if (quantity < maximum) {
                amount = quantity;
            } else {
                amount = maximum;
            }
            func_0019B710(&D_8021A124, amount);
        }
        break;
    }

    case 10:
    case 11:
    case 12: {
        s32 result = func_00134830(D_80219F00.prompt_code,
                                  D_80219F00.arg_10, 0, 0,
                                  D_80219F00.prompt_variant, 0x70);
        if (result != 2) {
            func_0015DF10(6);
            if (D_80219D24 == 10) {
                D_80219D24 = 3;
            } else if (D_80219D24 == 11) {
                D_80219D24 = 5;
            } else {
                u8 remaining = D_8021A124 & 0x7F;
                s32 index;
                func_0019B63C((u16)D_80219F00.selected_id, remaining);
                    for (index = 0; index < 10 && (u8)remaining != 0;
                         index++) {
                        u8 *slot = &g_func_001957D0_source_records[D_801F3658].field_0D[index];
                        if (*slot == 0) {
                            *slot = D_80219F08;
                            remaining--;
                        }
                    }
                D_80219F00.motion_flags = 1;
                D_80219D24 = 5;
            }
        }
        break;
    }

    case 13: {
        if (D_80219F00.tick >= 9) {
            D_80219F00.arg_10 =
                (void *)func_000453e0((u16)D_80219F00.selected_id);
            D_80219F00.prompt_code = 0x57;
            D_80219F00.prompt_variant = 2;
            D_80219D24 = 11;
        }
        goto tick_increment;
    }

    case 14: {
        func_00052284(D_80219F00.object_28);
        func_00052284(D_80219F00.object_2C);
        func_00052284(D_80219F00.object_30);
        func_00052284(D_80219F00.object_34);
        if (D_80219F00.object_38 != 0) {
            func_00052284(D_80219F00.object_38);
        }
        if (D_80219F00.object_3C != 0) {
            func_00052284(D_80219F00.object_3C);
        }
        D_80219F00.object_28 = 0;
        D_80219F00.object_2C = 0;
        D_80219F00.object_30 = 0;
        D_80219F00.object_34 = 0;
        D_80219F00.object_38 = 0;
        D_80219F00.object_3C = 0;
        D_80219D24 = 15;
        D_80219F00.tick = 0;
        break;
    }

    case 15: {
        if (D_80219F00.tick >= 11) {
            func_0019BCA0();
            resource_free(D_80219F00.category_rows);
            D_80219D24 = 0;
        }
        goto tick_increment;
    }
    }
    goto widget_motion;
tick_increment:
    tick_next = D_80219F00.tick + 1;
tick_store:
    D_80219F00.tick = tick_next;
widget_motion:

    /* The two motion directions share the rectangle update and return. */
    {
        u8 flags = D_80219F04;
        void *widget_ref;
        s16 y;
        if (flags & 1) {
            widget_ref = &D_80219F00.object_30;
            if (*(void **)widget_ref != 0) {
                y = (s32)((f64)D_80219F30->field_32 - D_80219DA0);
                if (y < 0xB6) {
                    y = 0xB6;
                    D_80219F04 = flags & ~1;
                }
                goto update_widget;
            }
        }
        if (flags & 2) {
            widget_ref = &D_80219F00.object_30;
            if (*(void **)widget_ref != 0) {
                y = (s32)((f64)D_80219F30->field_32 + D_80219DA8);
                if (y >= 0x100) {
                    s32 cleared_flags = flags & ~2;
                    y = 0xFF;
                    D_80219F04 = cleared_flags;
                }
                goto update_widget;
            }
        }
        goto widget_done;
update_widget:
        {
            /* The slot address becomes the actual object pointer for this call. */
            widget_ref = *(void **)widget_ref;
            func_0005219c(widget_ref, 0x18, y, 0x128,
                            (s16)(y + 0x28));
        }
widget_done:
        ;
    }
    return D_80219D24;
}
