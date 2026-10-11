typedef signed char s8;
typedef signed int s32;
typedef unsigned char u8;
typedef unsigned int u32;

extern u32 func_801D96A0(u32);

void func_0021CB30(u32 remaining)
{
    volatile u32 stack_gap[2];
    s8 *context;
    s8 *output_context;
    s32 consumed;
    u32 cursor;
    u32 amount;

    context = *(s8 **)0x801CE8C0;
    consumed = 0;
    cursor = 0;
    if (*(u32 *)(context + 0x814) == 0) {
        goto done;
    }
loop:
    amount = *(u8 *)(context + cursor + 0x12);
    if (amount >= remaining) {
        goto done;
    }
    remaining -= amount;
    consumed += amount;
    cursor = func_801D96A0(cursor);
    context = *(s8 **)0x801CE8C0;
    if (cursor < *(u32 *)(context + 0x814)) {
        goto loop;
    }

done:
    output_context = *(s8 **)0x801CE8C0;
    *(s32 *)(output_context + 8) = consumed + remaining;
    *(u32 *)(output_context + 4) = remaining;
    *(u32 *)(output_context + 0x810) = cursor;
}
