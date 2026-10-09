typedef signed char s8;
typedef unsigned char u8;
typedef unsigned int u32;

u32 func_0021CA88(u32 remaining)
{
    volatile u32 stack_gap[2];
    s8 *context;
    s8 *base;
    u32 cursor;
    u32 amount;
    u32 type;
    u32 type_16;
    u32 type_38;

    context = *(s8 **)0x801CE8C0;
    cursor = 0;
    if (*(u32 *)(context + 0x814) == 0) {
        goto done;
    }
    type_16 = 0x16;
    type_38 = 0x38;
    base = context;

loop:
    amount = *(u8 *)(context + cursor + 0x12);
    if (amount >= remaining) {
        goto done;
    }
    type = *(u8 *)(context + cursor + 0x10);
    remaining -= amount;
    if (type == type_16) {
        goto type_16_record;
    }
    if (type == type_38) {
        goto type_38_record;
    }
    goto regular_record;

type_16_record:
    cursor += *(u8 *)(context + cursor + 0x13) + 4;
    goto next_record;

type_38_record:
    cursor += *(u8 *)(context + cursor + 0x13);
    goto next_record;

regular_record:
    cursor += ((u8 *)0x801E5C70)[*(u8 *)(base + cursor + 0x10)];

next_record:
    context = *(s8 **)0x801CE8C0;
    if (cursor < *(u32 *)(context + 0x814)) {
        goto loop;
    }

done:
    return cursor;
}
