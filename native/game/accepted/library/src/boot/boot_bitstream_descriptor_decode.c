/* Unpack a descriptor-driven list of record arrays from a bit buffer. */
#include "bitstream.h"

void boot_bitstream_descriptor_decode(u8 *buffer, BitRecordSet *set)
{
    u8 *base;
    int j;
    unsigned int i;
    BitField *field;
    int size;
    unsigned int value;
    u8 *dst;

    bits_open(buffer);
    for (j = 0; (base = set[j].base) != 0; j++) {
        for (i = 0; i < set[j].count; i++, base += set[j].stride) {
            for (field = set[j].fields; (size = field->size) != 0; field++) {
                dst = base + field->offset;
                value = bits_read(field->bits);
                if (size & 0x80) {
                    /* signed field: the value's upper bits are set */
                    size &= 0x7F;
                    value |= -(1 << field->bits);
                }
                while (--size >= 0) {
                    *dst++ = value >> (size * 8);
                }
            }
        }
    }
}
