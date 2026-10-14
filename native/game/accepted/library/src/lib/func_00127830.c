typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
#define FIELD(p, type, off) (*(type *)((u8 *)(p) + (off)))
extern u8 D_800E7AC5;
extern u8 D_8018F481, D_801F1038;
extern u8 D_801951B0[];
extern u8 D_801E7E15[], D_80196A38[], D_801EE7D0[];
extern s32 D_801F1070, D_801F367C;
extern u8 *D_801F0CB0[];
extern u8 *boot_command_stream_dispatch(u32, s32, s32, s32);
extern void boot_command_stream_resource_node_dispatch(u32, s32, s32);
extern char *strncpy(char *, const char *, u32);
extern void *resource_alloc(u32);
extern void resource_free(void *);
extern void memset_00023780(void *, s32);
extern void func_00023940(void *, ...);
extern s32 func_0005c110(s32);
extern s32 func_0016C454(void);

void func_00127830(s32 refresh)
{
    u8 members[80];
    s32 index;
    s32 count;
    s32 unit_index;
    s32 offset;
    u32 copy_offset;
    u8 *descriptor_resource;
    volatile u8 *descriptor;
    u8 *records;
    u8 *destination;
    u8 *tail;
    u16 *flags;
    u8 *scan, *copy;
    u8 **units, **unit_table;
    u8 *buffer_start, *buffer_last;
    s32 record_id;
    u32 record_offset;
    s32 addend;
    s32 scenario, special;
    s32 limit;
    u32 context_offset;
    u8 *context, *record1, *record2, *old, *allocation, *end, *terminator_slot;
    s32 lo, hi, add_lo, add_hi;
    u32 high;
    u32 field_value;
    u8 member, metadata;

    /* The separate negation folds away after CSE, preserving the retail
     * base-first addition in KMC 2.7.2. Address sums below use target-width
     * integers for the same operand-order requirement. */
    context = &D_800E7AC5;
    context_offset = *context;
    context_offset <<= 2;
    context_offset = -context_offset;
    context = (u8 *)((u32)context - context_offset);
    D_801F1070 = FIELD(FIELD(context, u8 *, -0x35), s32, 0x18);
    scenario = D_8018F481;
    if ((scenario & 0xFF) == 0x27) {
        D_801F1070 = 3;
    } else if ((u32)(scenario - 0x2F) < 2 || (u32)(scenario - 0x3C) < 2) {
        D_801F1070 = 6;
    } else if ((scenario & 0xFF) == 0x33) {
        D_801F1070 = 4;
    }

    if (!refresh) {
        /* Single-pass scopes retain KMC's loop-note allocation and scheduling
         * context without adding native instructions. */
        do {
            special = D_801E7E15[D_8018F481 * 12];
            descriptor_resource = boot_command_stream_dispatch(0x021AF9F8, 2, -3, special);
            index = 0;
            destination = D_801951B0;
        } while (0);
        records = boot_command_stream_dispatch(0x021AF9F8, 2, -3, 0);
        descriptor_resource++;

        if (D_801F1070 > 0) {
            do {
                flags = (u16 *)(D_801951B0 + 28);
            } while (0);
            descriptor = descriptor_resource;
            /* Keep this one-time tail setup before the backward-jump label.
             * KMC then leaves the packed-byte cursor intact. Starting the
             * scope after the label instead pins the first byte load ahead
             * of the copy arguments. The tail cursor is biased to offset0x20. */
            do {
                tail = D_801951B0 + 32;
record_loop:
                /* The high-byte qualifiers and ordinary low-byte reads keep
                 * KMC's retail load widening and addition forms. */
                hi = descriptor[0];
                lo = ((u8 *)descriptor)[1];
                add_hi = descriptor[2];
                add_lo = ((u8 *)descriptor)[3];
                hi <<= 8;
                hi += lo;
                record_id = hi - 1;
                record_offset = (record_id & 0xFFFF) * 28;
                add_hi <<= 8;
                addend = add_hi + add_lo;
                strncpy((char *)destination,
                        (const char *)(records + (record_offset + 1)), 21);
                if (!D_801F1038 &&
                    (record1 = (u8 *)(record_offset + (u32)records),
                     high = record1[25],
                     FIELD(tail, u16, -4) = high,
                     FIELD(tail, u16, -4) = record1[26] + (high << 8) + addend,
                     func_0005c110(0x11))) {
                    *flags = (*flags & 0xFFF9) | 2;
                }
                destination += 36;
                flags = (u16 *)((u8 *)flags + 36);
                {
                    record2 = (u8 *)(((record_id & 0xFFFF) * 28) + (u32)records);
                    FIELD(tail, u8, -11) = 0;
                    FIELD(tail, s32, -8) = 0;
                    metadata = record2[24];
                    descriptor += 4;
                    index++;
                    FIELD(tail, u8, -2) = metadata;
                    field_value = record2[23] | (record2[22] << 8);
                    limit = D_801F1070;
                    FIELD(tail, u16, 0) = field_value;
                }
                tail += 36;
                if (index < limit) {
                    goto record_loop;
                }
            } while (0);
        }
        boot_command_stream_resource_node_dispatch(0x021AF9F8, 2, -17);
        if (func_0016C454()) {
            memset_00023780(D_80196A38, 32);
        }
    } else {
        index = 0;
        if (D_801F1070 > 0) {
            unit_table = D_801F0CB0;
            buffer_start = members;
            buffer_last = (u8 *)((u32)members - 1);
            offset = 0;
site_loop:
            {
                unit_index = 0;
                count = 0;
                if (D_801F367C > 0) {
                    units = unit_table;
                    scan = buffer_start;
                    do {
                        if (FIELD(*units, s32, 0x74) == index) {
                            /* Preserve the retail counter/cursor allocation. */
                            do {
                                *scan++ = unit_index;
                                count++;
                            } while (0);
                        }
                        unit_index++;
                        units++;
                    } while (unit_index < D_801F367C);
                }
                old = FIELD((void *)(0x80190000U + offset), u8 *volatile, 0x51C8);
                if (old) {
                    resource_free(old);
                    FIELD((void *)(0x80190000U + offset), u8 *volatile, 0x51C8) = 0;
                }
                if (count) {
                    FIELD((void *)(0x80190000U + offset), u8, 0x51C5) = count;
                    allocation = resource_alloc(count + 1);
                    FIELD((void *)(0x80190000U + offset), u8 *volatile, 0x51C8) = allocation;
                    if (!allocation) {
                        func_00023940(D_801EE7D0);
                    }
                    /* A separate pointer keeps the terminator's lifetime out
                     * of the reverse-copy loop's pointer quantity. */
                    terminator_slot = FIELD((void *)(0x80190000U + offset), u8 *volatile, 0x51C8);
                    copy_offset = offset;
                    copy = (u8 *)((u32)count + (u32)buffer_last);
                    terminator_slot += count;
                    *terminator_slot = 0xFF;
                    do {
                        member = *copy;
                        copy = (u8 *)((u32)copy - 1);
                        end = (u8 *)((u32)count + (u32)FIELD((void *)(0x80190000U + copy_offset), u8 *volatile, 0x51C8));
                        count--;
                        end[-1] = member;
                    } while (count);
                    goto site_next;
                }
                /* Both paths share advance. This single-pass cleanup scope
                 * retains KMC's allocation weights for the backward loop. */
                do {
                    FIELD((void *)(0x80190000U + offset), u8, 0x51C5) = 0;
                    FIELD((void *)(0x80190000U + offset), u8 *volatile, 0x51C8) = 0;
site_next:
                    index++;
                    offset += 36;
                    if (index < D_801F1070) {
                        goto site_loop;
                    }
                } while (0);
            }
        }
    }
}
