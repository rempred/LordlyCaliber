/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				huf.c -- new static Huffman									*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* Adapted from LHa for UNIX 1.14c decode_c_st1; see ../LOCAL_CHANGES.md.
 * Complete preserved physical owner. */
#include "huf.h"

unsigned short func_0000D248(void)
{
	unsigned short  j, mask;

	if (blocksize == 0) {
		blocksize = getbits(16);
		read_pt_len(NT, TBIT, 3);
		read_c_len();
		read_pt_len(np, pbit, -1);
	}
	blocksize--;
	j = c_table[bitbuf >> 4];
	if (j < NC)
		fillbuf(c_len[j]);
	else {
		fillbuf(12);
		mask = 1 << (16 - 1);
		do {
			if (bitbuf & mask)
				j = right[j];
			else
				j = left[j];
			mask >>= 1;
		} while (j >= NC);
		fillbuf(c_len[j] - 12);
	}
	return j;
}
