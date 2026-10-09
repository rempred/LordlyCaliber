/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				shuf.c -- extract static Huffman coding						*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
#include "shuf.h"

static __inline__ void
ready_made(method)
	int             method;
{
	int             i, j;
	unsigned int    code, weight;
	int            *tbl;

	tbl = fixed[method];
	j = *tbl++;
	weight = 1 << (16 - j);
	code = 0;
	for (i = 0; i < np; i++) {
		while (*tbl == i) {
			j++;
			tbl++;
			weight >>= 1;
		}
		pt_len[i] = j;
		pt_code[i] = code;
		code += weight;
	}
}

static __inline__ void
read_tree_p(/*void*/)
{				/* read tree from file */
	int             i, c;

	i = 0;
	while (i < NP) {
		pt_len[i] = getbits(LENFIELD);
		if (++i == 3 && pt_len[0] == 1 && pt_len[1] == 1 && pt_len[2] == 1) {
			c = getbits(MAX_DICBIT - 6);
			for (i = 0; i < NP; i++)
				c_len[i] = 0;
			for (i = 255; i >= 0; i--)
				c_table[i] = c;
			return;
		}
	}
}

unsigned short
decode_c_st0(/*void*/)
{
	int             i, j;
	/* OB64 has distinct entry gate and st0 count: preserve ROM, not upstream. */

	if (g_lha_blocksize == 0) {	/* read block head */
		g_lha_st0_blocksize = getbits(BUFBITS);	/* read block blocksize */
		read_tree_c();
		if (getbits(1)) {
			read_tree_p();
		}
		else {
			ready_made(1);
		}
		make_table(NP, pt_len, 8, pt_table);
	}
	g_lha_st0_blocksize--;
	j = c_table[bitbuf >> 4];
	if (j < N1)
		fillbuf(c_len[j]);
	else {
		fillbuf(12);
		i = bitbuf;
		do {
			if ((short) i < 0)
				j = right[j];
			else
				j = left[j];
			i <<= 1;
		} while (j >= N1);
		fillbuf(c_len[j] - 12);
	}
	if (j == N1 - 1)
		j += getbits(EXTRABITS);
	return j;
}
