/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				shuf.c -- extract static Huffman coding						*/
/*																			*/
/*		Modified          		Nobutaka Watazaki							*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
#include "shuf.h"

void
read_tree_c( /*void*/ )
{				/* read tree from file */
	int             i, c;

	i = 0;
	while (i < N1) {
		if (getbits(1))
			c_len[i] = getbits(LENFIELD) + 1;
		else
			c_len[i] = 0;
		if (++i == 3 && c_len[0] == 1 && c_len[1] == 1 && c_len[2] == 1) {
			c = getbits(CBIT);
			for (i = 0; i < N1; i++)
				c_len[i] = 0;
			for (i = 0; i < 4096; i++)
				c_table[i] = c;
			return;
		}
	}
	make_table(N1, c_len, 12, c_table);
}
