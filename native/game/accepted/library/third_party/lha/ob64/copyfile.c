/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				util.c -- LHarc Util										*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* Additional upstream util.c history retained below. */
/*
 * util.c - part of LHa for UNIX Feb 26 1992 modified by Masaru Oki Mar  4
 * 1992 modified by Masaru Oki #ifndef USESTRCASECMP added. Mar 31 1992
 * modified by Masaru Oki #ifdef NOMEMSET added.
 */
#include "huf.h"

/* Local game adaptation: 2048-byte binary copy; diagnostics return normally. */
long func_0000C990(void *f1, void *f2, long size, int crc_flg)
{
    unsigned short xsize;
    char *buf;
    long rsize = 0;

    if ((buf = (char *)func_00001330(2048)) == 0)
        func_00023940(g_lha_diag_format, g_lha_diag_prefix,
                     g_lha_diag_context, g_lha_err_alloc);
    g_lha_crc = 0;
    while (size > 0) {
        xsize = (size > 2048) ? 2048 : size;
        if (func_0000F970(buf, 1, xsize, f1) != xsize)
            func_00023940(g_lha_diag_format, g_lha_diag_prefix,
                         g_lha_diag_context, g_lha_err_read);
        if (f2) {
            if (func_0000F9D8(buf, 1, xsize, f2) != xsize)
                func_00023940(g_lha_diag_format, g_lha_diag_prefix,
                             g_lha_diag_context, g_lha_err_write);
        }
        if (crc_flg) {
            calccrc((unsigned char *)buf, xsize);
        }
        rsize += xsize;
        size -= xsize;
    }
    func_000016C4(buf);
    return rsize;
}
