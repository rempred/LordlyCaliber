/* ------------------------------------------------------------------------ */
/* LHa for UNIX    															*/
/*				dhuf.c -- Dynamic Hufffman routine							*/
/*																			*/
/*		Modified          		H.Yoshizaki									*/
/*																			*/
/*	Ver. 1.14 	Source All chagned				1995.01.14	N.Watazaki		*/
/* ------------------------------------------------------------------------ */
/* OB64 adaptation of LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#define LHA_DEFINES_E6F8
#include "dhuf.h"

unsigned short
boot_decode_huffman_symbol(void)
{
	int             c;
	short           buf, cnt;

	c = g_lha_child[ROOT_C];
	buf = g_lha_bitbuf;
	cnt = 0;
	do {
		c = g_lha_child[c - (buf < 0)];
		buf <<= 1;
		if (++cnt == 16) {
			LHA_FILLBUF(16);
			buf = g_lha_bitbuf;
			cnt = 0;
		}
	} while (c > 0);
	LHA_FILLBUF(cnt);
	c = ~c;
	/* update_c is inlined in the game. */
	{
		int             q;

		if (g_lha_freq[ROOT_C] == 0x8000) {
			boot_decode_huffman_tree_update(0, g_lha_n_max * 2 - 1);
		}
		g_lha_freq[ROOT_C]++;
		q = g_lha_s_node[c];
		do {
			q = func_0000DFF4(q);
		} while (q != ROOT_C);
	}
	if (c == g_lha_n1)
		c += func_0000C65C(8);
	return c;
}

static unsigned short
func_0000E6F8(void)
{
	int             c;
	short           buf, cnt;

	while (g_lha_count > g_lha_nextcount) {
		boot_decode_huffman_tree_update_entry(g_lha_nextcount / 64);
		if ((g_lha_nextcount += 64) >= g_lha_nn)
			g_lha_nextcount = 0xffffffff;
	}
	c = g_lha_child[ROOT_P];
	buf = g_lha_bitbuf;
	cnt = 0;
	while (c > 0) {
		c = g_lha_child[c - (buf < 0)];
		buf <<= 1;
		if (++cnt == 16) {
			LHA_FILLBUF(16);
			buf = g_lha_bitbuf;
			cnt = 0;
		}
	}
	LHA_FILLBUF(cnt);
	c = (~c) - N_CHAR;
	/* update_p is inlined in the game. */
	{
		int             q;

		if (g_lha_total_p == 0x8000) {
			boot_decode_huffman_tree_update(ROOT_P, g_lha_most_p + 1);
			g_lha_total_p = g_lha_freq[ROOT_P];
			g_lha_freq[ROOT_P] = 0xffff;
		}
		q = g_lha_s_node[c + N_CHAR];
		while (q != ROOT_P) {
			q = func_0000DFF4(q);
		}
		g_lha_total_p++;
	}

	return (c << 6) + func_0000C65C(6);
}
