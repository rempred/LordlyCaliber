/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				dhuf.c -- Dynamic Hufffman routine							*/
/*																			*/
/*		Modified          		H.Yoshizaki									*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* OB64 adaptation of LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#include "dhuf.h"

void
boot_decode_huffman_tree_init(void)
{
	g_lha_n_max = 286;
	g_lha_maxmatch = MAXMATCH;
	func_0000C838();
	boot_decode_huffman_tree_reconstruct();
	/* start_p_dyn is inlined in the game. */
	{
		g_lha_freq[ROOT_P] = 1;
		g_lha_child[ROOT_P] = ~(N_CHAR);
		g_lha_s_node[N_CHAR] = ROOT_P;
		g_lha_edge[g_lha_block[ROOT_P] = g_lha_stock[g_lha_avail++]] = ROOT_P;
		g_lha_most_p = ROOT_P;
		g_lha_total_p = 0;
		g_lha_nn = 1 << g_lha_dicbit;
		g_lha_nextcount = 64;
	}
}
