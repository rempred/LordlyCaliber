/*
 * Owner 0xF734..0xF808: decode_start_lz5() [larc.c].
 * Adapted from LHa for UNIX 1.14c (third_party/lha); notices in its README.
 * Local changes against 1.14c: none in this body.
 */
#include "lha.h"

void
func_0000F734(void)     /* decode_start_lz5 */
{
    int i;

    flagcnt = 0;
    for (i = 0; i < 256; i++)
        memset(&text[i * 13 + 18], i, 13);
    for (i = 0; i < 256; i++)
        text[256 * 13 + 18 + i] = i;
    for (i = 0; i < 256; i++)
        text[256 * 13 + 256 + 18 + i] = 255 - i;
    memset(&text[256 * 13 + 512 + 18], 0, 128);
    memset(&text[256 * 13 + 512 + 128 + 18], ' ', 128 - 18);
}
