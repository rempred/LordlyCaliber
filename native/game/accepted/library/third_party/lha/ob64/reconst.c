/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				dhuf.c -- Dynamic Hufffman routine							*/
/*																			*/
/*		Modified          		H.Yoshizaki									*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* OB64 adaptation of LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#define LHA_DEFINES_DFF4
#include "dhuf.h"

void
boot_decode_huffman_tree_update(int start, int end)
{
	int             i, j, k, l, b = g_lha_block[start];
	unsigned int    f, g;

	for (i = j = start; i < end; i++) {
		if ((k = g_lha_child[i]) < 0) {
			g_lha_freq[j] = (g_lha_freq[i] + 1) / 2;
			g_lha_child[j] = k;
			j++;
		}
		if (g_lha_edge[b = g_lha_block[i]] == i) {
			g_lha_stock[--g_lha_avail] = b;
		}
	}
	j--;
	i = end - 1;
	l = end - 2;
	while (i >= start) {
		while (i >= l) {
			g_lha_freq[i] = g_lha_freq[j];
			g_lha_child[i] = g_lha_child[j];
			i--, j--;
		}
		f = g_lha_freq[l] + g_lha_freq[l + 1];
		for (k = start; f < g_lha_freq[k]; k++);
		while (j >= k) {
			g_lha_freq[i] = g_lha_freq[j];
			g_lha_child[i] = g_lha_child[j];
			i--, j--;
		}
		g_lha_freq[i] = f;
		g_lha_child[i] = l + 1;
		i--;
		l -= 2;
	}
	f = 0;
	for (i = start; i < end; i++) {
		if ((j = g_lha_child[i]) < 0)
			g_lha_s_node[~j] = i;
		else
			g_lha_parent[j] = g_lha_parent[j - 1] = i;
		if ((g = g_lha_freq[i]) == f) {
			g_lha_block[i] = b;
		}
		else {
			g_lha_edge[b = g_lha_block[i] = g_lha_stock[g_lha_avail++]] = i;
			f = g;
		}
	}
}

static int
func_0000DFF4(int p)
{
	int             b, q, r, s;

	b = g_lha_block[p];
	if ((q = g_lha_edge[b]) != p) {	/* swap for leader */
		r = g_lha_child[p];
		s = g_lha_child[q];
		g_lha_child[p] = s;
		g_lha_child[q] = r;
		if (r >= 0)
			g_lha_parent[r] = g_lha_parent[r - 1] = q;
		else
			g_lha_s_node[~r] = q;
		if (s >= 0)
			g_lha_parent[s] = g_lha_parent[s - 1] = p;
		else
			g_lha_s_node[~s] = p;
		p = q;
		goto Adjust;
	}
	else if (b == g_lha_block[p + 1]) {
Adjust:
		g_lha_edge[b]++;
		if (++g_lha_freq[p] == g_lha_freq[p - 1]) {
			g_lha_block[p] = g_lha_block[p - 1];
		}
		else {
			g_lha_edge[g_lha_block[p] = g_lha_stock[g_lha_avail++]] = p;	/* create g_lha_block */
		}
	}
	else if (++g_lha_freq[p] == g_lha_freq[p - 1]) {
		g_lha_stock[--g_lha_avail] = b;	/* delete g_lha_block */
		g_lha_block[p] = g_lha_block[p - 1];
	}
	return g_lha_parent[p];
}
