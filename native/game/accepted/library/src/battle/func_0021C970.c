typedef unsigned char u8;
typedef unsigned int u32;

struct ActionStreamContext {
    u32 field_000;
    u32 field_004;
    u32 field_008;
    u8 pad_00C[0x804];
    u32 cursor;
    u32 length;
};

u32 func_0021C970(u32 offset)
{
    struct ActionStreamContext *context;
    u8 *record;
    u8 *scan_base;
    u8 type;

    context = *(struct ActionStreamContext **)0x801CE8C0;
    if (offset >= context->length) {
        goto fail;
    }
    record = (u8 *)((u32)context + offset);
    type = record[0x10];
    if (type != 0x16) {
        goto check_type_38;
    }
    offset += record[0x13] + 4;
    goto check_end;

check_type_38:
    if (type != 0x38) {
        goto check_type_0;
    }
    offset += record[0x13];
    goto check_end;

check_type_0:
    if (type != 0) {
        goto regular_record;
    }
    scan_base = (u8 *)context;
    do {
        offset++;
    } while (*(u8 *)((u32)scan_base + offset + 0x10) == 0);
    return offset;

regular_record:
    offset += ((u8 *)0x801E5C70)[type];

check_end:
    if (offset < (*(struct ActionStreamContext **)0x801CE8C0)->length) {
        return offset;
    }

fail:
    return -1;
}
