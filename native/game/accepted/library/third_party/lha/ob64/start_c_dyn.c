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
boot_decode_huffman_tree_reconstruct(void)
{
	int             i, j, f;

	g_lha_n1 = (g_lha_n_max >= 256 + g_lha_maxmatch - THRESHOLD + 1) ? 512 : g_lha_n_max - 1;
	for (i = 0; i < TREESIZE_C; i++) {
		g_lha_stock[i] = i;
		g_lha_block[i] = 0;
	}
	for (i = 0, j = g_lha_n_max * 2 - 2; i < g_lha_n_max; i++, j--) {
		g_lha_freq[j] = 1;
		g_lha_child[j] = ~i;
		g_lha_s_node[i] = j;
		g_lha_block[j] = 1;
	}
	g_lha_avail = 2;
	g_lha_edge[1] = g_lha_n_max - 1;
	i = g_lha_n_max * 2 - 2;
	while (j >= 0) {
		f = g_lha_freq[j] = g_lha_freq[i] + g_lha_freq[i - 1];
		g_lha_child[j] = i;
		g_lha_parent[i] = g_lha_parent[i - 1] = j;
		if (f == g_lha_freq[j + 1]) {
			g_lha_edge[g_lha_block[j] = g_lha_block[j + 1]] = j;
		}
		else {
			g_lha_edge[g_lha_block[j] = g_lha_stock[g_lha_avail++]] = j;
		}
		i -= 2;
		j--;
	}
}
