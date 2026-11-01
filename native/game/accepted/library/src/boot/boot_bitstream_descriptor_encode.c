/*
 * HYBRID_C physical-owner fallback: readable C covers the reviewed 548-byte body.
 * The source-authored assembler below retains the original UNKNOWN 16-byte suffix.
 * The final .size represents the unchanged 564-byte physical owner envelope only;
 * it does not establish a logical function, entry, padding or semantics for the suffix.
 */
#include "bitstream.h"

void boot_bitstream_descriptor_encode(u8 *buffer, BitRecordSet *set)
{
    u8 *base;
    int j;
    unsigned int i;
    BitField *field;
    int size;
    unsigned int value;
    u8 *src;

    bits_open(buffer);
    for (j = 0; (base = set[j].base) != 0; j++) {
        for (i = 0; i < set[j].count; i++, base += set[j].stride) {
            for (field = set[j].fields; (size = field->size & 0x7F) != 0; field++) {
                src = base + field->offset;
                value = 0;
                for (; size > 0; size--) {
                    value = (value << 8) | *src++;
                }
                bits_write(field->bits, value);
            }
        }
    }
    bits_write(7, 0);
}

/* Exact original ROM 0x4AB8..0x4AC8, without new labels or alignment. */
asm(
    ".word 0x00000000\n"
    ".word 0x00000000\n"
    ".word 0x03E00008\n"
    ".word 0x00000000\n"
    ".size boot_bitstream_descriptor_encode, 564\n"
);
