typedef unsigned char u8;
typedef unsigned int u32;

extern u8 * volatile D_801CE8C0;

void func_0021D374(u32 offset)
{
    u8 *context;
    u8 *final_context;
    u8 *final_record;
    u8 *initial_context;
    u8 *regular_context;
    u8 *regular_record;
    u8 *record;
    u32 adjustment;
    u32 cursor;
    u32 size;
    u32 type;

    initial_context = D_801CE8C0;
    record = initial_context + offset;
    type = record[0x10];
    adjustment = record[0x11];
    if (type == 0x16) {
        goto type_16;
    }
    if (type == 0x38) {
        goto type_38;
    }
    goto regular;

type_16:
    size = record[0x13] + 4;
    goto shift;

type_38:
    size = record[0x13];
    goto shift;

regular:
    regular_context = D_801CE8C0;
    regular_record = regular_context + offset;
    size = ((u8 *)0x801E5C70)[regular_record[0x10]];

shift:
    context = D_801CE8C0;
    cursor = offset;
    if (offset < *(u32 *)(context + 0x814) - size) {
        do {
            u8 *destination;
            u8 *source;
            u32 source_offset;

            source_offset = cursor + size;
            source = context + source_offset;
            destination = context + cursor;
            destination[0x10] = source[0x10];
            context = D_801CE8C0;
            cursor++;
        } while (cursor < *(u32 *)(context + 0x814) - size);
    }
    final_context = D_801CE8C0;
    *(u32 *)(final_context + 0x814) -= size;
    final_record = final_context + offset;
    final_record[0x11] += adjustment;
}
