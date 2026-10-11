/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				shuf.c -- extract static Huffman coding						*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
#include "shuf.h"

unsigned short
decode_p_st0(/*void*/)
{
	int             i, j;

	j = pt_table[bitbuf >> 8];
	if (j < np) {
		fillbuf(pt_len[j]);
	}
	else {
		fillbuf(8);
		i = bitbuf;
		do {
			if ((short) i < 0)
				j = right[j];
			else
				j = left[j];
			i <<= 1;
		} while (j >= np);
		fillbuf(pt_len[j] - 8);
	}
	return (j << 6) + getbits(6);
}
