/*
 * Owner 0xC310..0xC778: decode() [slide.c], make_crctable() and getbits() [crcio.c].
 * Adapted from LHa for UNIX 1.14c (third_party/lha); notices in its README.
 *
 * Local changes against 1.14c:
 *   decode()        no NULL check after malloc; returns nothing; the lzs offset test uses
 *                   method 6 (this decode_define has no lh6 entry).
 *   make_crctable() fills a table reached through a pointer (upstream: static array).
 *   getbits()       fillbuf() expanded in place.
 *   The remaining crcio.c functions (calccrc, fillbuf, putcode, putbits, fread_crc) are
 *   not separate bodies in this owner.
 */
#define LHA_DEFINES_C310
#include "lha.h"

void
decode(struct interfacing *interface)
{
    int i, j, k, c, dicsiz1, offset;

    infile = interface->infile;
    outfile = interface->outfile;
    dicbit = interface->dicbit;
    origsize = interface->original;
    compsize = interface->packed;
    decode_set = decode_define[interface->method - 1];
    crc = 0;
    prev_char = -1;
    dicsiz = 1 << dicbit;
    text = (unsigned char *) malloc(dicsiz);
    memset(text, ' ', dicsiz);
    decode_set.decode_start();
    dicsiz1 = dicsiz - 1;
    offset = (interface->method == 6) ? 0x100 - 2 : 0x100 - 3;
    count = 0;
    loc = 0;
    while (count < origsize) {
        c = decode_set.decode_c();
        if (c <= UCHAR_MAX) {
            text[loc++] = c;
            if (loc == dicsiz) {
                fwrite_crc(text, dicsiz, outfile);
                loc = 0;
            }
            count++;
        }
        else {
            j = c - offset;
            i = (loc - decode_set.decode_p() - 1) & dicsiz1;
            count += j;
            for (k = 0; k < j; k++) {
                c = text[(i + k) & dicsiz1];
                text[loc++] = c;
                if (loc == dicsiz) {
                    fwrite_crc(text, dicsiz, outfile);
                    loc = 0;
                }
            }
        }
    }
    if (loc != 0) {
        fwrite_crc(text, loc, outfile);
    }
    free(text);
}

static void
make_crctable(void)
{
    unsigned int i, j, r;

    for (i = 0; i <= UCHAR_MAX; i++) {
        r = i;
        for (j = 0; j < CHAR_BIT; j++)
            if (r & 1)
                r = (r >> 1) ^ CRCPOLY;
            else
                r >>= 1;
        crctable[i] = r;
    }
}

static unsigned short
getbits(unsigned char n)
{
    unsigned short x;

    x = bitbuf >> (2 * CHAR_BIT - n);
    fillbuf(n);
    return x;
}
