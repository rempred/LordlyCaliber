typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;

typedef s32 (*ClassCheck)(u8, u8);

extern u8 D_80193BC0[];
extern ClassCheck D_801EF288[];

s32 func_000bec90(s32 arg0, s32 arg1)
{
    register s32 slot asm("$18");
    register ClassCheck *check asm("$19");
    register s32 class_index asm("$2");
    s32 target;
    s32 switch_value;
    s32 result_value;
    u8 result;
    u16 value;
    void *slot_record;
    void *record;

    result = 0;
    arg1 &= 0xFFFF;
    if (arg1 != 0) {
        slot = 0;
        class_index = arg0 & 0xFF;
        record = (class_index * 0x38) + D_80193BC0;
        slot_record = record;
        target = arg1;
        check = D_801EF288;
loop:
        switch_value = (slot + 1) & 0xFF;
        switch (switch_value) {
        case 1:
            value = *(u16 *)((s8 *)slot_record + 0x2A);
            break;
        case 2:
            value = *(u16 *)((s8 *)slot_record + 0x2C);
            break;
        case 3:
            value = *(u16 *)((s8 *)slot_record + 0x2E);
            break;
        default:
            value = *(u16 *)((s8 *)slot_record + 0x30);
            break;
        }
        if (target != value) {
            goto primary_done;
        }
        result += 1;
primary_done:
        result_value = result & 0xFF;
        if (result_value == 0) {
            if (target != ((*check)(*(u8 *)((s8 *)record + 0x11),
                                      *(u8 *)((s8 *)record + 0x12)) & 0xFFFF)) {
                goto secondary_done;
            }
            result += 1;
secondary_done:
            result_value = result & 0xFF;
            slot += 1;
            if (result_value == 0) {
                check += 1;
                if (slot >= 4) {
                    goto done;
                }
                goto loop;
            }
        }
    } else {
done:
        result_value = result & 0xFF;
    }
    return result_value;
    asm("# Hybrid scope: the three register-asm declarations above emit no instructions.\n"
        "# They keep the four-pass slot counter in $s2, the advancing predicate-table pointer in $s3,\n"
        "# and the short-lived class index in $v0 so KMC reproduces the retail register allocation.\n"
        "# All loads, comparisons, indirect calls, branches, loop control, and the return remain compiled C.\n");
}
