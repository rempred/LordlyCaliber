typedef unsigned char u8;
typedef unsigned int u32;
typedef signed int s32;

u8 *func_80232E44(s32 arg0, s32 arg1);

s32 func_002a3198(s32 arg0, s32 table_index, s32 wanted_index, s32 selector,
                  u8 *out0, u8 *out1, u8 *out2)
{
    u8 *base;
    u8 *record;
    u32 record_index;
    u32 record_count;
    u32 record_offset;
    s32 record_size;
    u32 stack_shape[2];

    base = func_80232E44(arg0, selector);
    if (base != 0) {
        record_offset = *(u32 *)(base + table_index * 4);
        record = (u8 *)(record_offset + (u32)base);
    } else {
        record = (u8 *)0;
    }

    record_count = *record++;
    record_index = 0;
    if (record_count != 0) {
        do {
            if (record_index == (u32)wanted_index) {
                record_size = *(u8 *)(0x801CEEE0 + *record);
                switch (record_size) {
                case 2:
                    if (out0 != 0) {
                        *out0 = *(volatile u8 *)(record + 1);
                    }
                    break;
                case 3:
                    if (out0 != 0) {
                        *out0 = *(volatile u8 *)(record + 1);
                    }
                    if (out1 != 0) {
                        *out1 = *(volatile u8 *)(record + 2);
                    }
                    break;
                case 4:
                    if (out0 != 0) {
                        *out0 = *(volatile u8 *)(record + 1);
                    }
                    if (out1 != 0) {
                        *out1 = *(volatile u8 *)(record + 2);
                    }
                    if (out2 != 0) {
                        *out2 = *(volatile u8 *)(record + 3);
                    }
                    break;
                }
                return *(volatile u8 *)record;
            }
            record_index++;
            record += *(u8 *)(0x801CEEE0 + *record);
        } while (record_index < record_count);
    }
    return 0;
}

asm(".word 0\n.word 0\n.size func_002a3198, .-func_002a3198");
