/*
 * Owner 0x43D4..0x46F4 (six bodies in one preserved physical owner):
 *   43D4 clear per-slot counters under masked interrupts, 4450 open, 4480 read one bit,
 *   44F0 read n bits, 45A8 write one bit, 462C write n bits.
 */
#include "bitstream.h"

typedef struct BootSlotCounters {
    u8 pad_00[0x2C];
    int field_2C;
    int field_30;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    u8 pad_44[8];
    int field_4C;
} BootSlotCounters;

extern BootSlotCounters *g_boot_slot_counters[7]; /* 0x800A8218 */
extern int func_0008B820(int mask);                /* interrupt-mask set, returns previous */

void boot_bitstream_cursor_helpers(void)
{
    int saved;
    int i;

    saved = func_0008B820(1);
    for (i = 0; i < 7; i++) {
        g_boot_slot_counters[i]->field_2C = 0;
        g_boot_slot_counters[i]->field_30 = 0;
        g_boot_slot_counters[i]->field_34 = 0;
        g_boot_slot_counters[i]->field_38 = 0;
        g_boot_slot_counters[i]->field_3C = 0;
        g_boot_slot_counters[i]->field_40 = 0;
        g_boot_slot_counters[i]->field_4C = 0;
    }
    func_0008B820(saved);
}

static void func_00004450(u8 *buffer)
{
    g_bits_buffer = buffer;
    g_bits_pos = 0;
    g_bits_read_left = 0;
    g_bits_write_left = 8;
    g_bits_current = 0;
}

/* Separate returns preserve the pinned compiler's ABI result allocation. */
static unsigned int func_00004480(void)
{
    if (--g_bits_read_left >= 0)
        return (g_bits_current >> g_bits_read_left) & 1;
    g_bits_read_left = 7;
    g_bits_current = g_bits_buffer[g_bits_pos++];
    return g_bits_current >> 7;
}

static unsigned int func_000044F0(int n)
{
    unsigned int value = 0;

    while (g_bits_read_left < n) {
        n -= g_bits_read_left;
        value |= (g_bits_current & ((1 << g_bits_read_left) - 1)) << n;
        g_bits_current = g_bits_buffer[g_bits_pos++];
        g_bits_read_left = 8;
    }
    g_bits_read_left -= n;
    return value | ((g_bits_current >> g_bits_read_left) & ((1 << n) - 1));
}

static void func_000045A8(int bit)
{
    g_bits_write_left--;
    if (bit) {
        g_bits_current |= 1 << g_bits_write_left;
    }
    if (g_bits_write_left == 0) {
        g_bits_buffer[g_bits_pos++] = g_bits_current;
        g_bits_current = 0;
        g_bits_write_left = 8;
    }
}

static void func_0000462C(int n, unsigned int value)
{
    while (n >= g_bits_write_left) {
        n -= g_bits_write_left;
        g_bits_current |= (value >> n) & ((1 << g_bits_write_left) - 1);
        g_bits_buffer[g_bits_pos++] = g_bits_current;
        g_bits_current = 0;
        g_bits_write_left = 8;
    }
    g_bits_write_left -= n;
    g_bits_current |= (value & ((1 << n) - 1)) << g_bits_write_left;
}
