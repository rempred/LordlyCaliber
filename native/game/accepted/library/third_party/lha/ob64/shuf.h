/* Adapted from LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#ifndef OB64_LHA_SHUF_H
#define OB64_LHA_SHUF_H
#include "lha.h"

/* Legacy static Huffman declarations: address/type changes in ../LOCAL_CHANGES.md. */
#define N1 286
#define NP 14 /* OB64 ECF0 uses shared NP=MAX_DICBIT+1, not shuf.c's 128. */
#define MAX_DICBIT 13
#define MAXMATCH 256
#define LENFIELD 4
#define CBIT 9
#define EXTRABITS 8
#define BUFBITS 16
extern unsigned short g_lha_maxmatch;       /* 800AF39E */
extern unsigned short g_lha_blocksize;      /* 800AF3C6, st1 and st0 gate */
extern unsigned short g_lha_st0_blocksize;  /* 800AF3C8, st0 write/decrement */
extern int g_lha_n_max;                     /* 800AF3CC */
extern unsigned int g_lha_st0_np;           /* 800AF3E8 */
extern unsigned short *g_lha_left;          /* 800AF3FC */
extern unsigned short *g_lha_right;         /* 800AF400 */
extern unsigned char *g_lha_c_len;          /* 800AF404 */
extern unsigned char *g_lha_pt_len;         /* 800AF408 */
extern unsigned short *g_lha_c_table;       /* 800AF40C */
extern unsigned short *g_lha_pt_table;      /* 800AF410 */
extern unsigned short *g_lha_pt_code;       /* 800AF414 */
extern int g_lha_fixed[2][16];              /* 800A87CC */
extern char g_lha_bad_table_fmt[];          /* 800AE30C */
extern char g_lha_bad_table_prefix[];       /* 800AE31C */
extern char g_lha_bad_table_detail[];       /* 800AE338 */
extern char g_lha_bad_table_suffix[];       /* 800AE334 */
void boot_decode_build_huffman_table(short nchar, unsigned char bitlen[], short tablebits, unsigned short table[]);
void boot_decode_huffman_init(void);
void boot_decode_huffman_build_litlen_table(void);
void boot_decode_huffman_build_dist_table(void);
unsigned short boot_decode_huffman_codelengths(void);
unsigned short boot_decode_canonical_huffman_symbol(void);
void boot_decode_huffman_tree_reconstruct(void); /* upstream start_c_dyn, ROM D9B8 */
/* Aliases retain upstream expression for direct review. */
#define n_max g_lha_n_max
#define maxmatch g_lha_maxmatch
#define np g_lha_st0_np
#define fixed g_lha_fixed
#define left g_lha_left
#define right g_lha_right
#define c_len g_lha_c_len
#define pt_len g_lha_pt_len
#define c_table g_lha_c_table
#define pt_table g_lha_pt_table
#define pt_code g_lha_pt_code
#define make_table boot_decode_build_huffman_table
#define start_c_dyn boot_decode_huffman_tree_reconstruct
#define decode_start_st0 boot_decode_huffman_init
#define read_tree_c boot_decode_huffman_build_litlen_table
#define decode_start_fix boot_decode_huffman_build_dist_table
#define decode_c_st0 boot_decode_huffman_codelengths
#define decode_p_st0 boot_decode_canonical_huffman_symbol

#endif
