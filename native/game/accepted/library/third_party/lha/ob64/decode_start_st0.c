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
decode_start_st0( /*void*/ )
{
	n_max = 286;
	/* OB64 EAB4 clears the separate st0 count before bit initialization. */
	g_lha_st0_blocksize = 0;
	maxmatch = MAXMATCH;
	init_getbits();
	np = 1 << (MAX_DICBIT - 6);
}
