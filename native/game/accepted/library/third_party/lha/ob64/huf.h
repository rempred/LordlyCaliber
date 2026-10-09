/* Adapted from LHa for UNIX 1.14c; see ../LOCAL_CHANGES.md. */
#ifndef OB64_LHA_HUF_H
#define OB64_LHA_HUF_H
#include "lha.h"

/* Local static-Huffman extension: pointers in retail, not upstream arrays. */
extern unsigned short g_lha_blocksize; /* 0x800AF3C6 */
extern unsigned short *g_lha_left;     /* 0x800AF3FC */
extern unsigned short *g_lha_right;    /* 0x800AF400 */
extern unsigned char *g_lha_c_len;     /* 0x800AF404 */
extern unsigned char *g_lha_pt_len;    /* 0x800AF408 */
extern unsigned short *g_lha_c_table;  /* 0x800AF40C */
extern unsigned short *g_lha_pt_table; /* 0x800AF410 */
extern char g_lha_diag_format[], g_lha_diag_prefix[], g_lha_diag_context[];
extern char g_lha_err_alloc[], g_lha_err_read[], g_lha_err_write[];
extern unsigned int func_0000F970(void *, unsigned int, unsigned int, void *);
extern void func_0000C024(short, unsigned char *, short, unsigned short *);
void func_0000CB4C(short nn, short nbit, short i_special);
void func_0000CEB8(void);
unsigned short func_0000D248(void);
unsigned short func_0000D600(void);

/* Preserve upstream expressions through descriptive aliases. */
#define NC 510
#define NT 19
#define CBIT 9
#define TBIT 5
#define np 14 /* Retail has fixed LH4/LH5 position alphabet. */
#define pbit 4
#define blocksize g_lha_blocksize
#define left g_lha_left
#define right g_lha_right
#define c_len g_lha_c_len
#define pt_len g_lha_pt_len
#define c_table g_lha_c_table
#define pt_table g_lha_pt_table
#define make_table func_0000C024
#define read_pt_len func_0000CB4C
#define read_c_len func_0000CEB8
#endif
