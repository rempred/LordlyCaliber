/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				huf.c -- new static Huffman									*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* Adapted from LHa for UNIX 1.14c decode_p_st1; see ../LOCAL_CHANGES.md.
 * Complete preserved physical owner. */
#include "huf.h"

unsigned short func_0000D600(void)
{
	unsigned short  j, mask;

	j = pt_table[bitbuf >> (16 - 8)];
	if (j < np)
		fillbuf(pt_len[j]);
	else {
		fillbuf(8);
		mask = 1 << (16 - 1);
		do {
			if (bitbuf & mask)
				j = right[j];
			else
				j = left[j];
			mask >>= 1;
		} while (j >= np);
		fillbuf(pt_len[j] - 8);
	}
	if (j != 0)
		j = (1 << (j - 1)) + getbits(j - 1);
	return j;
}
