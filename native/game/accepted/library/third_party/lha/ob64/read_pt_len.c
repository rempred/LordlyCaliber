/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				huf.c -- new static Huffman									*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* Adapted from LHa for UNIX 1.14c read_pt_len; see ../LOCAL_CHANGES.md.
 * Complete preserved physical owner. */
#include "huf.h"

void func_0000CB4C(short nn, short nbit, short i_special)
{
	short           i, c, n;

	n = getbits(nbit);
	if (n == 0) {
		c = getbits(nbit);
		for (i = 0; i < nn; i++)
			pt_len[i] = 0;
		for (i = 0; i < 256; i++)
			pt_table[i] = c;
	}
	else {
		i = 0;
		while (i < n) {
			c = bitbuf >> (16 - 3);
			if (c == 7) {
				unsigned short  mask = 1 << (16 - 4);
				while (mask & bitbuf) {
					mask >>= 1;
					c++;
				}
			}
			fillbuf((c < 7) ? 3 : c - 3);
			pt_len[i++] = c;
			if (i == i_special) {
				c = getbits(2);
				while (--c >= 0)
					pt_len[i++] = 0;
			}
		}
		while (i < nn)
			pt_len[i++] = 0;
		make_table(nn, pt_len, 8, pt_table);
	}
}
