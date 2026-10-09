/*
 * Shared types and inline helpers for the boot bit-packing cursor.
 *
 * One byte-buffer cursor serves both directions: a reader (g_bits_read_left
 * counts down from 0 and refills) and a writer (g_bits_write_left counts down
 * from 8 and flushes).  The descriptor coders at ROM 0x46F4/0x4894 expand the
 * cursor helpers in place, so the helpers are also available as static inline
 * functions here.  Addresses are accepted early-boot runtime addresses.
 */
#ifndef BITSTREAM_H
#define BITSTREAM_H

typedef unsigned char u8;

extern u8 *g_bits_buffer;              /* 0x800AEFB0 */
extern int g_bits_pos;                 /* 0x800AEFB4 */
extern int g_bits_read_left;           /* 0x800AEFB8 */
extern int g_bits_write_left;          /* 0x800AEFBC */
extern unsigned int g_bits_current;    /* 0x800AEFC0 */

/* Field descriptor: `size` bytes at base+offset hold a `bits`-wide value; 0x80 in size marks a signed field. */
typedef struct BitField {
    u8 offset;
    u8 size;
    u8 bits;
} BitField;

/* Record-array descriptor; a list of these ends with base == 0. */
typedef struct BitRecordSet {
    u8 *base;
    int stride;
    BitField *fields;       /* ends with size == 0 */
    unsigned int count;
} BitRecordSet;

static inline void bits_open(u8 *buffer)
{
    g_bits_buffer = buffer;
    g_bits_pos = 0;
    g_bits_read_left = 0;
    g_bits_write_left = 8;
    g_bits_current = 0;
}

static inline unsigned int bits_read(int n)
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

static inline void bits_write(int n, unsigned int value)
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

#endif
