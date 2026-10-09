/*
 * Owner 0xC778..0xC990: fwrite_crc(), init_getbits() [crcio.c] and convdelim() [util.c].
 * Adapted from LHa for UNIX 1.14c (third_party/lha); notices in its README.
 *
 * Local changes against 1.14c:
 *   fwrite_crc()   calccrc() expanded in place (no reading_size); no verify/text mode;
 *                  fatal_error() expanded to the printf-style report with the retail strings.
 *   init_getbits() fillbuf() expanded in place; no EUC cache reset.
 *   convdelim()    no MULTIBYTE_CHAR handling.
 */
#define LHA_DEFINES_C778
#include "lha.h"

/* Retail rodata strings at 0x800AE30C, 0x800AE324, 0x800AE334 and 0x800AE348. */
extern const char lha_fatal_format[];   /* "LHa: %s%s %s\n" */
extern const char lha_fatal_prefix[];   /* "Fatal error:" */
extern const char lha_empty_string[];   /* "" */
extern const char lha_write_error[];    /* "File write error\n" */

void
fwrite_crc(unsigned char *p, int n, void *fp)
{
    calccrc(p, n);
    if (fp) {
        if (fwrite(p, 1, n, fp) < n)
            lha_printf(lha_fatal_format, lha_fatal_prefix, lha_empty_string, lha_write_error);
    }
}

static void
init_getbits(void)
{
    bitbuf = 0;
    subbitbuf = 0;
    bitcount = 0;
    fillbuf(2 * CHAR_BIT);
}

static unsigned char *
convdelim(unsigned char *path, unsigned char delim)
{
    unsigned char c;
    unsigned char *p;

    for (p = path; (c = *p) != 0; p++) {
        if (c == '\\' || c == DELIM || c == DELIM2) {
            *p = delim;
            path = p + 1;
        }
    }
    return path;
}
