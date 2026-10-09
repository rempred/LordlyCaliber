typedef unsigned char u8;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

s32 func_0020C104(void *);
s32 func_0020C32C(void *);
s32 func_0020D444(s32, s32);
void func_0021D230(s32, s32, void *, s32);
void func_0021D2C0(s32, s32, void *, s32, s32, s32, s32);
s32 func_0022257C(void *, s32, s32 *);
void func_0022A280(void *, s32, u8);
void func_0022A414(void *);
void func_0022A964(void *, s32, s32, s32, u8);

void func_0022ADFC(void *record, s32 offset, s32 time, s32 amount, s32 parameter, u8 flags, s32 mode, s32 adjust) {
    s32 event_code;
    s32 changed_bits;
    s32 first_count;
    s32 second_count;
    s32 next_time;
    s32 bit;
    s32 delta;
    u16 field20;
    u16 current20;
    u32 index;
    u8 *cursor;

    if (func_0020C32C(record) == 0) {
        delta = func_0022257C((u8 *)*(void **)record + 0x44, ((0 - (mode == 0)) & 0x19) | 0x11, 0) + offset;
        if (mode != 0) {
            if (adjust == 0) {
                event_code = 0xC;
                goto emit_event;
            }
            goto adjust_record;
        }
        event_code = 0x10;
        goto emit_event;
    }
    delta = 6;
    if (adjust != 0) {
adjust_record:
        func_0021D2C0(0x32, time, record, 0xFF, amount >> 8, amount & 0xFF, parameter);
        func_0022A414(record);
    } else {
        event_code = 0xC;
emit_event:
        func_0021D2C0(event_code, time, record, 0xFF, amount >> 8, amount & 0xFF, parameter);
    }
    if (func_0020C104(record) != 0) {
        field20 = *(u16 *)((u8 *)record + 0x20);
        if (amount >= (s32) field20) {
            goto zero_remaining;
        }
        first_count = func_0020D444(field20 - amount, *(u16 *)((u8 *)record + 0x22));
        second_count = func_0020D444(*(u16 *)((u8 *)record + 0x20), *(u16 *)((u8 *)record + 0x22));
        *(u16 *)((u8 *)record + 0x20) = (*(u16 *)((u8 *)record + 0x20) - amount);
        if (second_count != first_count) {
            changed_bits = ((1 << second_count) - 1) ^ ((1 << first_count) - 1);
            func_0021D230(0x13, time + delta, record, changed_bits);
            delta += func_0022257C((u8 *)*(void **)record + 0x44, 0x1D, 0);
            func_0021D230(0x15, time + delta, record, changed_bits);
            index = 0;
            cursor = record;
            do {
                bit = (changed_bits >> index) & 1;
                index += 1;
                if (bit) {
                    *(s32 *)((u8 *)cursor + 0) = 0;
                    *(s32 *)((u8 *)cursor + 0xC) = 0;
                }
                cursor += 4;
            } while (index < 3U);
            next_time = time + delta;
            if (*(s32 *)((u8 *)record + 0x94) < next_time) {
                *(s32 *)((u8 *)record + 0x94) = next_time;
            }
        }
        if (first_count != 0) {
            parameter &= 0xFF;
            time += delta;
            func_0022A964(record, time, mode, parameter, flags);
        }
    } else {
        current20 = *(u16 *)((u8 *)record + 0x20);
        if (amount < (s32) current20) {
            *(u16 *)((u8 *)record + 0x20) = (current20 - amount);
            time += delta;
            func_0022A964(record, time, mode, parameter & 0xFF, flags);
            return;
        }
zero_remaining:
        time += delta;
        func_0022A280(record, time, flags);
    }
}
