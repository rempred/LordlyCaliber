/*
 * Owner 0xF5A0..0xF618: decode_c_lzs(), decode_p_lzs() [larc.c].
 * Adapted from LHa for UNIX 1.14c (third_party/lha); notices in its README.
 * Local changes against 1.14c: none in these bodies (matchpos is retail data, not file-static).
 */
#include "lha.h"

unsigned short
func_0000F5A0(void)     /* decode_c_lzs */
{
    if (getbits(1)) {
        return getbits(8);
    }
    else {
        matchpos = getbits(11);
        return getbits(4) + 0x100;
    }
}

static unsigned short
func_0000F5F8(void)     /* decode_p_lzs */
{
    return (loc - matchpos - MAGIC0) & 0x7ff;
}
