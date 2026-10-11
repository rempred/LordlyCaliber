#ifndef LORDLY_SAVE_CHECKSUMS_H
#define LORDLY_SAVE_CHECKSUMS_H
/* Portable adaptations of accepted func_00005E84/00005EC4 and
 * func_00005F00/00005F60. See accepted/gameplay-imports.json for the unchanged
 * complete producer. The seed is the physical SRAM offset, or zero for Pak.
 * The original count and accumulators are halfwords; preserve that narrowing.
 * Explicit byte reads work on both the PC and the source-built N64 runtime.
 */
static unsigned int lc_save_byte_sum(const unsigned char *bytes,
                                     unsigned int count, unsigned int seed)
{
    unsigned int total = 0, index = 0;
    count &= 0xFFFFU;
    if (count != 0) {
        do {
            total += bytes[index & 0xFFFFU];
            index++;
        } while ((index & 0xFFFFU) < count);
    }
    return (total + seed) & 0xFFFFU;
}
static unsigned int lc_save_bit_sum(const unsigned char *bytes,
                                    unsigned int count, unsigned int seed)
{
    unsigned short total = 0, index = 0;
    count &= 0xFFFFU;
    if (count != 0) {
        do {
            unsigned int value = bytes[index];
            unsigned short bit;
            for (bit = 0; bit < 8; bit++) {
                if (value & 1) total++;
                value >>= 1;
            }
            index++;
        } while (index < count);
    }
    return (unsigned short)(total + seed);
}
#endif
