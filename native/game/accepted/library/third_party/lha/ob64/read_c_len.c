/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				huf.c -- new static Huffman									*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* Adapted from LHa for UNIX 1.14c read_c_len; see ../LOCAL_CHANGES.md.
 * Complete preserved physical owner. */
#include "huf.h"

void func_0000CEB8(void)
{
	short           i, c, n;

	n = getbits(CBIT);
	if (n == 0) {
		c = getbits(CBIT);
		for (i = 0; i < NC; i++)
			c_len[i] = 0;
		for (i = 0; i < 4096; i++)
			c_table[i] = c;
	}
	else {
		i = 0;
		while (i < n) {
			c = pt_table[bitbuf >> (16 - 8)];
			if (c >= NT) {
				unsigned short  mask = 1 << (16 - 9);
				do {
					if (bitbuf & mask)
						c = right[c];
					else
						c = left[c];
					mask >>= 1;
				} while (c >= NT);
			}
			fillbuf(pt_len[c]);
			if (c <= 2) {
				if (c == 0)
					c = 1;
				else if (c == 1)
					c = getbits(4) + 3;
				else
					c = getbits(CBIT) + 20;
				while (--c >= 0)
					c_len[i++] = 0;
			}
			else
				c_len[i++] = c - 2;
		}
		while (i < NC)
			c_len[i++] = 0;
		make_table(NC, c_len, 12, c_table);
	}
}
