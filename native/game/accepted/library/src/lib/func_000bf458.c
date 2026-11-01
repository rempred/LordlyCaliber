typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

typedef s32 (*ClassCheck)(s32, s32);

s32 func_000415fc(s32);
s32 func_000bedb8(s32, s32, u8);
extern u8 D_80193BC0[];
extern ClassCheck D_801EF288[];

void func_000bf458(void)
{
    register s32 available_count asm("$19");
    register s32 exact_class_count asm("$21");
    s32 check_result;
    s32 class_group_index;
    s32 class_id;
    register s32 index asm("$16");
    u16 selected_class;
    register u32 eligible_count asm("$18");
    u32 group_limit;
    u8 alternate_class;
    s32 current_class;
    s32 record_class;
    s32 class_offset;
    s32 group_class;
    u8 compared_class;
    u8 *data;
    register void *cursor asm("$17");
    register s32 total asm("$3");

    available_count = 0;
    eligible_count = 0;
    exact_class_count = 0;
    index = 1;
    cursor = D_80193BC0 + 0x38;
    do {
        record_class = ((u8 *)cursor)[0x11];
        class_id = record_class & 0xFF;
        alternate_class = *(u8 *)(class_id + 0x801EF3B8);
        compared_class = ((u8 *)cursor)[0x12];
        if (alternate_class != 0) {
            class_offset = class_id * 0x48;
            asm volatile(
                "# Hybrid scope: these three instructions perform one class-table byte load.\n"
                "# $at receives class_records[0].field_45's upper address, adds the\n"
                "# class's 0x48-byte record offset, and loads field_45 into the C result.\n"
                "# KMC otherwise hoists the table base into a saved register, changing\n"
                "# the retail frame and loop schedule even though the C value is equal.\n"
                "# The explicit $s3/$s5/$s2/$s0/$s1 and $v1 bindings elsewhere emit\n"
                "# no opcodes; they preserve retail's long-lived counters, cursors, and\n"
                "# final sum allocation after pure C selected different registers.\n"
                "lui $1,%%hi(g_func_0019554C_class_records+0x45) # load the class field_45 table address high half into $at\n"
                "addu $1,$1,%1 # add the selected class's 0x48-byte record offset to $at\n"
                "lbu %0,%%lo(g_func_0019554C_class_records+0x45)($1) # load and zero-extend field_45 for that class record\n"
                : "=r" (current_class)
                : "r" (class_offset)
                : "$1");
            if (current_class == compared_class) {
                compared_class = alternate_class;
            }
        }
        if (record_class != 0) {
            selected_class = *(u16 *)(*(u8 **)0x80196AF8 + 0x5E8);
            if (compared_class == selected_class) {
                exact_class_count += 1;
                goto accepted_class;
            }
            if (func_000bedb8(index & 0xFF,
                              selected_class & 0xFF,
                              record_class) & 0xFF) {
                eligible_count += 1;
accepted_class:
                available_count += 1;
            }
        }
        index += 1;
        cursor = (u8 *)cursor + 0x38;
    } while (index < 0x64);

    index = 0;
    cursor = D_801EF288;
    group_class = (*(u8 **)0x80196AF8)[0x5E9];
    do {
        check_result = (*(ClassCheck *)cursor)(group_class, group_class) & 0xFFFF;
        if (check_result != 0) {
            class_group_index = (func_000415fc(check_result) & 0xFF) * 4;
            group_limit = *(u8 *)(class_group_index + 0x80196B03) -
                          *(u8 *)(class_group_index + 0x80196B02);
            if (group_limit >= eligible_count) {
                group_limit = eligible_count;
            }
            eligible_count = group_limit;
        }
        index += 1;
        cursor = (u8 *)cursor + 4;
    } while (index < 4);

    data = *(u8 **)0x80196AF8;
    total = exact_class_count + eligible_count;
    *(s16 *)(data + 0x1BF6) = exact_class_count;
    *(s16 *)(data + 0x1BF8) = total;
    *(s16 *)(data + 0x1BFA) = available_count;
}
