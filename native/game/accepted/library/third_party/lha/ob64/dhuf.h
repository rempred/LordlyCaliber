/* Adapted from LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#ifndef OB64_LHA_DHUF_H
#define OB64_LHA_DHUF_H
#include "lha.h"

/* Dynamic-tree additions, ROM pointer slots rather than embedded arrays. */
#define N_CHAR 314
#define TREESIZE_C 628
#define ROOT_C 0
#define ROOT_P 628
#define MAXMATCH 256
extern short * g_lha_child; /* 0x800AF418 */
extern short * g_lha_parent; /* 0x800AF41C */
extern short * g_lha_block; /* 0x800AF420 */
extern short * g_lha_edge; /* 0x800AF424 */
extern short * g_lha_stock; /* 0x800AF428 */
extern short * g_lha_s_node; /* 0x800AF42C */
extern unsigned short * g_lha_freq; /* 0x800AF430 */
extern unsigned short g_lha_total_p; /* 0x800AF3D0 */
extern int g_lha_avail; /* 0x800AF3D4 */
extern int g_lha_n1; /* 0x800AF3D8 */
extern int g_lha_most_p; /* 0x800AF3DC */
extern int g_lha_nn; /* 0x800AF3E0 */
extern unsigned long g_lha_nextcount; /* 0x800AF3E4 */
extern int g_lha_n_max; /* 0x800AF3CC */
extern unsigned short g_lha_maxmatch; /* 0x800AF39E */

void boot_decode_huffman_tree_reconstruct(void); /* start_c_dyn */
void boot_decode_huffman_tree_init(void); /* decode_start_dyn */
void boot_decode_huffman_tree_update(int start, int end); /* reconst */
#ifdef LHA_DEFINES_DFF4
static int func_0000DFF4(int p);
#else
int func_0000DFF4(int p);
#endif /* swap_inc */
void boot_decode_huffman_tree_update_entry(int p); /* make_new_node */
unsigned short boot_decode_huffman_symbol(void); /* decode_c_dyn */
#ifdef LHA_DEFINES_E6F8
static unsigned short func_0000E6F8(void);
#else
unsigned short func_0000E6F8(void);
#endif /* decode_p_dyn; complete earlier entry */
#endif
