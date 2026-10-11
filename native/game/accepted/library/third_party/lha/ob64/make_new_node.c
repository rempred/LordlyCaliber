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
boot_decode_huffman_tree_update_entry(int p)
{
	int             q, r;

	r = g_lha_most_p + 1;
	q = r + 1;
	g_lha_s_node[~(g_lha_child[r] = g_lha_child[g_lha_most_p])] = r;
	g_lha_child[q] = ~(p + N_CHAR);
	g_lha_child[g_lha_most_p] = q;
	g_lha_freq[r] = g_lha_freq[g_lha_most_p];
	g_lha_freq[q] = 0;
	g_lha_block[r] = g_lha_block[g_lha_most_p];
	if (g_lha_most_p == ROOT_P) {
		g_lha_freq[ROOT_P] = 0xffff;
		g_lha_edge[g_lha_block[ROOT_P]]++;
	}
	g_lha_parent[r] = g_lha_parent[q] = g_lha_most_p;
	g_lha_edge[g_lha_block[q] = g_lha_stock[g_lha_avail++]] = g_lha_s_node[p + N_CHAR] = g_lha_most_p = q;
	/* update_p is inlined in the game. */
	{
		int             q;

		if (g_lha_total_p == 0x8000) {
			boot_decode_huffman_tree_update(ROOT_P, g_lha_most_p + 1);
			g_lha_total_p = g_lha_freq[ROOT_P];
			g_lha_freq[ROOT_P] = 0xffff;
		}
		q = g_lha_s_node[p + N_CHAR];
		while (q != ROOT_P) {
			q = func_0000DFF4(q);
		}
		g_lha_total_p++;
	}
}
