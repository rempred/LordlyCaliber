/*
 * Owner 0xF618..0xF734: decode_start_lzs(), decode_c_lz5(), decode_p_lz5() [larc.c].
 * Adapted from LHa for UNIX 1.14c (third_party/lha); notices in its README.
 * Local changes against 1.14c: getc() reads the in-memory stream (see lha.h).
 */
#include "lha.h"

void
func_0000F618(void)     /* decode_start_lzs */
{
    init_getbits();
}

static unsigned short
func_0000F634(void)     /* decode_c_lz5 */
{
    int c;

    if (flagcnt == 0) {
        flagcnt = 8;
        flag = getc(infile);
    }
    flagcnt--;
    c = getc(infile);
    if ((flag & 1) == 0) {
        matchpos = c;
        c = getc(infile);
        matchpos += (c & 0xf0) << 4;
        c &= 0x0f;
        c += 0x100;
    }
    flag >>= 1;
    return c;
}

static unsigned short
func_0000F714(void)     /* decode_p_lz5 */
{
    return (loc - matchpos - MAGIC5) & 0xfff;
}
