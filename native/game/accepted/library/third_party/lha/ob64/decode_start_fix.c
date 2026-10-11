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

void
decode_start_fix(/*void*/)
{
	n_max = 314;
	maxmatch = 60;
	init_getbits();
	np = 1 << (12 - 6);
	start_c_dyn();
	ready_made(0);
	make_table(np, pt_len, 8, pt_table);
}
